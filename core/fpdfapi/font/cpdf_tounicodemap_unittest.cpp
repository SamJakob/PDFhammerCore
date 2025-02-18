// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/font/cpdf_tounicodemap.h"

#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CPDFToUnicodeMapTest, StringToCode) {
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<0001>"), testing::Optional(1u));
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<c2>"), testing::Optional(194u));
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<A2>"), testing::Optional(162u));
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<Af2>"),
              testing::Optional(2802u));
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<FFFFFFFF>"),
              testing::Optional(4294967295u));

  // Whitespaces within the string are ignored.
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<00\n0\r1>"),
              testing::Optional(1u));
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<c 2>"),
              testing::Optional(194u));
  EXPECT_THAT(CPDF_ToUnicodeMap::StringToCode("<A2\r\n>"),
              testing::Optional(162u));

  // Integer overflow
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("<100000000>").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("<1abcdFFFF>").has_value());

  // Invalid string
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("<>").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("12").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("<12").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("12>").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("<1-7>").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("00AB").has_value());
  EXPECT_FALSE(CPDF_ToUnicodeMap::StringToCode("<00NN>").has_value());
}

TEST(CPDFToUnicodeMapTest, StringToWideString) {
  EXPECT_EQ(L"", CPDF_ToUnicodeMap::StringToWideString(""));
  EXPECT_EQ(L"", CPDF_ToUnicodeMap::StringToWideString("1234"));
  EXPECT_EQ(L"", CPDF_ToUnicodeMap::StringToWideString("<c2"));
  EXPECT_EQ(L"", CPDF_ToUnicodeMap::StringToWideString("<c2D2"));
  EXPECT_EQ(L"", CPDF_ToUnicodeMap::StringToWideString("c2ab>"));

  WideString res = L"\xc2ab";
  EXPECT_EQ(res, CPDF_ToUnicodeMap::StringToWideString("<c2ab>"));
  EXPECT_EQ(res, CPDF_ToUnicodeMap::StringToWideString("<c2abab>"));
  EXPECT_EQ(res, CPDF_ToUnicodeMap::StringToWideString("<c2ab 1234>"));

  res += L"\xfaab";
  EXPECT_EQ(res, CPDF_ToUnicodeMap::StringToWideString("<c2abFaAb>"));
  EXPECT_EQ(res, CPDF_ToUnicodeMap::StringToWideString("<c2abFaAb12>"));
}

TEST(CPDFToUnicodeMapTest, HandleBeginBFCharBadCount) {
  {
    static constexpr uint8_t kInput1[] =
        "1 beginbfchar<1><0041><2><0042>endbfchar";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput1);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(0u, map.ReverseLookup(0x0041));
    EXPECT_EQ(0u, map.ReverseLookup(0x0042));
    EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(1u));
    EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(2u));
  }
  {
    static constexpr uint8_t kInput2[] =
        "3 beginbfchar<1><0041><2><0042>endbfchar";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput2);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(0u, map.ReverseLookup(0x0041));
    EXPECT_EQ(0u, map.ReverseLookup(0x0042));
    EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(1u));
    EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(2u));
  }
}

TEST(CPDFToUnicodeMapTest, HandleBeginBFRangeRejectsInvalidCidValues) {
  {
    static constexpr uint8_t kInput1[] =
        "1 beginbfrange<FFFFFFFF><FFFFFFFF>[<0041>]endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput1);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(L"", map.Lookup(0xffffffff));
  }
  {
    static constexpr uint8_t kInput2[] =
        "1 beginbfrange<FFFFFFFF><FFFFFFFF><0042>endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput2);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(L"", map.Lookup(0xffffffff));
  }
  {
    static constexpr uint8_t kInput3[] =
        "1 beginbfrange<FFFFFFFF><FFFFFFFF><00410042>endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput3);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(L"", map.Lookup(0xffffffff));
  }
  {
    static constexpr uint8_t kInput4[] =
        "1 beginbfrange<0001><10000>[<0041>]endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput4);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(L"", map.Lookup(0xffffffff));
    EXPECT_EQ(L"", map.Lookup(0x0001));
    EXPECT_EQ(L"", map.Lookup(0xffff));
    EXPECT_EQ(L"", map.Lookup(0x10000));
  }
  {
    static constexpr uint8_t kInput5[] =
        "1 beginbfrange<10000><10001>[<0041>]endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput5);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(L"", map.Lookup(0x10000));
    EXPECT_EQ(L"", map.Lookup(0x10001));
  }
  {
    static constexpr uint8_t kInput6[] =
        "1 beginbfrange<0006><0004>[<0041>]endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput6);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(L"", map.Lookup(0x0004));
    EXPECT_EQ(L"", map.Lookup(0x0005));
    EXPECT_EQ(L"", map.Lookup(0x0006));
  }
}

TEST(CPDFToUnicodeMapTest, HandleBeginBFRangeRejectsMismatchedBracket) {
  static constexpr uint8_t kInput[] = "1 beginbfrange<3><3>[<0041>}endbfrange";
  auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput);
  CPDF_ToUnicodeMap map(stream);
  EXPECT_EQ(0u, map.ReverseLookup(0x0041));
  EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(3u));
}

TEST(CPDFToUnicodeMapTest, HandleBeginBFRangeBadCount) {
  {
    static constexpr uint8_t kInput1[] =
        "1 beginbfrange<1><2><0040><4><5><0050>endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput1);
    CPDF_ToUnicodeMap map(stream);
    for (wchar_t unicode = 0x0039; unicode < 0x0053; ++unicode) {
      EXPECT_EQ(0u, map.ReverseLookup(unicode));
    }
    for (uint32_t charcode = 0; charcode < 7; ++charcode) {
      EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(charcode));
    }
  }
  {
    static constexpr uint8_t kInput2[] =
        "3 beginbfrange<1><2><0040><4><5><0050>endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput2);
    CPDF_ToUnicodeMap map(stream);
    for (wchar_t unicode = 0x0039; unicode < 0x0053; ++unicode) {
      EXPECT_EQ(0u, map.ReverseLookup(unicode));
    }
    for (uint32_t charcode = 0; charcode < 7; ++charcode) {
      EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(charcode));
    }
  }
}

TEST(CPDFToUnicodeMapTest, HandleBeginBFRangeGoodCount) {
  static constexpr uint8_t kInput[] =
      "2 beginbfrange<1><2><0040><4><5><0050>endbfrange";
  auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput);
  CPDF_ToUnicodeMap map(stream);
  EXPECT_EQ(0u, map.ReverseLookup(0x0039));
  EXPECT_EQ(1u, map.ReverseLookup(0x0040));
  EXPECT_EQ(2u, map.ReverseLookup(0x0041));
  EXPECT_EQ(0u, map.ReverseLookup(0x0042));
  EXPECT_EQ(0u, map.ReverseLookup(0x0049));
  EXPECT_EQ(4u, map.ReverseLookup(0x0050));
  EXPECT_EQ(5u, map.ReverseLookup(0x0051));
  EXPECT_EQ(0u, map.ReverseLookup(0x0052));
  EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(0u));
  EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(1u));
  EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(2u));
  EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(3u));
  EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(4u));
  EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(5u));
  EXPECT_EQ(0u, map.GetUnicodeCountByCharcodeForTesting(6u));
}

TEST(CPDFToUnicodeMapTest, InsertIntoMultimap) {
  {
    // Both the CIDs and the unicodes are different.
    static constexpr uint8_t kInput1[] =
        "2 beginbfchar<1><0041><2><0042>endbfchar";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput1);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(1u, map.ReverseLookup(0x0041));
    EXPECT_EQ(2u, map.ReverseLookup(0x0042));
    EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(1u));
    EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(2u));
  }
  {
    // The same CID with different unicodes.
    static constexpr uint8_t kInput2[] =
        "2 beginbfrange<0><0><0041><0><0><0042>endbfrange";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput2);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(0u, map.ReverseLookup(0x0041));
    EXPECT_EQ(0u, map.ReverseLookup(0x0042));
    EXPECT_EQ(2u, map.GetUnicodeCountByCharcodeForTesting(0u));
  }
  {
    // Duplicate mappings of CID 0 to unicode "A". There should be only 1 entry
    // in `m_Multimap`.
    static constexpr uint8_t kInput3[] =
        "1 beginbfrange<0><0>[<0041>]endbfrange\n"
        "1 beginbfchar<0><0041>endbfchar";
    auto stream = pdfium::MakeRetain<CPDF_Stream>(kInput3);
    CPDF_ToUnicodeMap map(stream);
    EXPECT_EQ(0u, map.ReverseLookup(0x0041));
    EXPECT_EQ(1u, map.GetUnicodeCountByCharcodeForTesting(0u));
  }
}

TEST(CPDFToUnicodeMapTest, NonBmpUnicodeLookup) {
  static constexpr uint8_t kInput[] = "1 beginbfchar<01><d841de76>endbfchar";
  CPDF_ToUnicodeMap map(pdfium::MakeRetain<CPDF_Stream>(kInput));
  EXPECT_EQ(L"\xd841\xde76", map.Lookup(0x01));
#if defined(WCHAR_T_IS_32_BIT)
  // TODO(crbug.com/374947848): Should work if wchar_t is 16-bit.
  // TODO(crbug.com/374947848): Should return 1u.
  EXPECT_EQ(0u, map.ReverseLookup(0x20676));
#endif
}
