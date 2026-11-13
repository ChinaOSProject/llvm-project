//===- EhFrame.cpp -------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// .eh_frame section contains information on how to unwind the stack when
// an exception is thrown. The section consists of sequence of CIE and FDE
// records. The linker needs to merge CIEs and associate FDEs to CIEs.
// That means the linker has to understand the format of the section.
//
// This file contains a few utility functions to read .eh_frame contents.
//
//===----------------------------------------------------------------------===//

#include "EhFrame.h"
#include "Config.h"
#include "InputFiles.h"
#include "InputSection.h"
#include "Relocations.h"
#include "Target.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/DataExtractor.h"

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::dwarf;
using namespace llvm::object;
using namespace lld;
using namespace lld::elf;

namespace {
class EhReader {
public:
  EhReader(InputSectionBase *s, ArrayRef<uint8_t> d)
      : isec(s), d(d), start(d.data()) {}
  uint8_t getFdeEncoding(CieEncodings *encodings = nullptr);
  bool hasLSDA();

private:
  template <class P> void errOn(const P *loc, const Twine &msg) {
    Ctx &ctx = isec->file->ctx;
    Err(ctx) << "corrupted .eh_frame: " << msg << "\n>>> defined in "
             << isec->getObjMsg((const uint8_t *)loc - isec->content().data());
  }

  uint8_t readByte();
  void skipBytes(size_t count);
  StringRef readString();
  void skipLeb128();
  void skipAugP(CieEncodings *encodings = nullptr);
  StringRef getAugmentation();

  InputSectionBase *isec;
  ArrayRef<uint8_t> d;
  const uint8_t *start;
};
}

// Read a byte and advance D by one byte.
uint8_t EhReader::readByte() {
  if (d.empty()) {
    errOn(d.data(), "unexpected end of CIE");
    return 0;
  }
  uint8_t b = d.front();
  d = d.slice(1);
  return b;
}

void EhReader::skipBytes(size_t count) {
  if (d.size() < count)
    errOn(d.data(), "CIE is too small");
  else
    d = d.slice(count);
}

// Read a null-terminated string.
StringRef EhReader::readString() {
  const uint8_t *end = llvm::find(d, '\0');
  if (end == d.end()) {
    errOn(d.data(), "corrupted CIE (failed to read string)");
    return {};
  }
  StringRef s = toStringRef(d.slice(0, end - d.begin()));
  d = d.slice(s.size() + 1);
  return s;
}

// Skip an integer encoded in the LEB128 format.
// Actual number is not of interest because only the runtime needs it.
// But we need to be at least able to skip it so that we can read
// the field that follows a LEB128 number.
void EhReader::skipLeb128() {
  const uint8_t *errPos = d.data();
  while (!d.empty()) {
    uint8_t val = d.front();
    d = d.slice(1);
    if ((val & 0x80) == 0)
      return;
  }
  errOn(errPos, "corrupted CIE (failed to read LEB128)");
}

static size_t getAugPSize(Ctx &ctx, unsigned enc) {
  switch (enc & 0x0f) {
  case DW_EH_PE_absptr:
  case DW_EH_PE_signed:
    return ctx.arg.wordsize;
  case DW_EH_PE_udata2:
  case DW_EH_PE_sdata2:
    return 2;
  case DW_EH_PE_udata4:
  case DW_EH_PE_sdata4:
    return 4;
  case DW_EH_PE_udata8:
  case DW_EH_PE_sdata8:
    return 8;
  }
  return 0;
}

void EhReader::skipAugP(CieEncodings *encodings) {
  uint32_t encodingOffset = d.data() - start;
  uint8_t enc = readByte();
  if (encodings) {
    encodings->personalityEncodingOffset = encodingOffset;
    encodings->personalityEncoding = enc;
    encodings->personalityOffset = d.data() - start;
  }
  if ((enc & 0xf0) == DW_EH_PE_aligned)
    return errOn(d.data() - 1, "DW_EH_PE_aligned encoding is not supported");
  size_t size = getAugPSize(isec->getCtx(), enc);
  if (size == 0)
    return errOn(d.data() - 1, "unknown FDE encoding");
  if (size >= d.size())
    return errOn(d.data() - 1, "corrupted CIE");
  d = d.slice(size);
}

uint8_t elf::getFdeEncoding(EhSectionPiece *p, CieEncodings *encodings) {
  return EhReader(p->sec, p->data()).getFdeEncoding(encodings);
}

uint32_t elf::getFdeLsdaOffset(EhSectionPiece *p, uint8_t encoding) {
  Ctx &ctx = p->sec->getCtx();
  DataExtractor data(p->data(), ctx.arg.isLE, ctx.arg.wordsize);
  DataExtractor::Cursor cursor(8 + 2 * getAugPSize(ctx, encoding));
  uint64_t augmentationSize = data.getULEB128(cursor);
  if (Error error = cursor.takeError())
    Fatal(ctx) << p->sec << ": invalid FDE augmentation: "
               << toString(std::move(error));
  if (augmentationSize == 0)
    return 0;
  if (augmentationSize < ctx.arg.wordsize ||
      !data.isValidOffsetForDataOfSize(cursor.tell(), augmentationSize))
    Fatal(ctx) << p->sec << ": invalid FDE augmentation size";
  return cursor.tell();
}

bool elf::hasLSDA(const EhSectionPiece &p) {
  return EhReader(p.sec, p.data()).hasLSDA();
}

StringRef EhReader::getAugmentation() {
  skipBytes(8);
  int version = readByte();
  if (version != 1 && version != 3) {
    errOn(d.data() - 1,
          "FDE version 1 or 3 expected, but got " + Twine(version));
    return {};
  }

  StringRef aug = readString();

  // Skip code and data alignment factors.
  skipLeb128();
  skipLeb128();

  // Skip the return address register. In CIE version 1 this is a single
  // byte. In CIE version 3 this is an unsigned LEB128.
  if (version == 1)
    readByte();
  else
    skipLeb128();
  return aug;
}

uint8_t EhReader::getFdeEncoding(CieEncodings *encodings) {
  // 读取地址编码，需要元数据时同时记录各字段的位置。
  StringRef aug = getAugmentation();
  uint8_t fdeEncoding = DW_EH_PE_absptr;
  for (char c : aug) {
    if (c == 'R') {
      if (!encodings)
        return readByte();
      encodings->fdeEncodingOffset = d.data() - start;
      fdeEncoding = readByte();
      encodings->fdeEncoding = fdeEncoding;
    } else if (c == 'z')
      skipLeb128();
    else if (c == 'L') {
      uint32_t encodingOffset = d.data() - start;
      uint8_t encoding = readByte();
      if (encodings) {
        encodings->lsdaEncodingOffset = encodingOffset;
        encodings->lsdaEncoding = encoding;
      }
    } else if (c == 'P')
      skipAugP(encodings);
    else if (c != 'B' && c != 'S' && c != 'G') {
      errOn(aug.data(), "unknown .eh_frame augmentation string: " + aug);
      break;
    }
  }
  return fdeEncoding;
}

bool EhReader::hasLSDA() {
  StringRef aug = getAugmentation();
  for (char c : aug) {
    if (c == 'L')
      return true;
    if (c == 'z')
      skipLeb128();
    else if (c == 'P')
      skipAugP();
    else if (c == 'R')
      readByte();
    else if (c != 'B' && c != 'S' && c != 'G') {
      errOn(aug.data(), "unknown .eh_frame augmentation string: " + aug);
      break;
    }
  }
  return false;
}
