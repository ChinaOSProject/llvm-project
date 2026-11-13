//===- EhFrame.h ------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLD_ELF_EHFRAME_H
#define LLD_ELF_EHFRAME_H

#include "lld/Common/LLVM.h"
#include "llvm/BinaryFormat/Dwarf.h"

namespace lld::elf {
struct EhSectionPiece;

struct CieEncodings {
  uint8_t fdeEncoding = llvm::dwarf::DW_EH_PE_absptr;
  uint8_t lsdaEncoding = llvm::dwarf::DW_EH_PE_omit;
  uint8_t personalityEncoding = llvm::dwarf::DW_EH_PE_omit;
  uint32_t fdeEncodingOffset = 0;
  uint32_t lsdaEncodingOffset = 0;
  uint32_t personalityEncodingOffset = 0;
  uint32_t personalityOffset = 0;
  llvm::StringRef augmentation;
  uint32_t augmentationSizeOffset = 0;
  uint32_t augmentationDataOffset = 0;
  uint64_t augmentationSize = 0;
};

uint8_t getFdeEncoding(EhSectionPiece *p, CieEncodings *encodings = nullptr);
uint32_t getFdeLsdaOffset(EhSectionPiece *p, uint8_t encoding);
bool hasLSDA(const EhSectionPiece &p);
}

#endif
