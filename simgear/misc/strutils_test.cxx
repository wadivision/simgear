// -*- coding: utf-8 -*-
//
// Unit tests for functions inside the strutils package

#include <string>
#include <vector>
#include <utility>              // std::move()
#include <fstream>              // std::ifstream
#include <sstream>              // std::ostringstream
#include <ios>                  // std::dec, std::hex
#include <limits>               // std::numeric_limits
#include <typeinfo>             // typeid()
#include <cstdint>              // uint16_t, uintmax_t, etc.
#include <cstdlib>              // _set_errno() on Windows
#include <cerrno>
#include <cassert>

#include <simgear/misc/test_macros.hxx>
#include <simgear/compiler.h>
#include <simgear/misc/strutils.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/constants.h>

using std::string;
using std::vector;

namespace strutils = simgear::strutils;

void test_strip()
{
  string a("abcd");
  SG_CHECK_EQUAL(strutils::strip(a), a);
  SG_CHECK_EQUAL(strutils::strip(" a "), "a");
  SG_CHECK_EQUAL(strutils::lstrip(" a  "), "a  ");
  SG_CHECK_EQUAL(strutils::rstrip("\ta "), "\ta");

  // Check internal spacing is preserved
  SG_CHECK_EQUAL(strutils::strip("\t \na \t b\r \n "), "a \t b");
}

void test_stripTrailingNewlines()
{
  SG_CHECK_EQUAL(strutils::stripTrailingNewlines("\rfoobar\n\r\n\n\r\r"),
                 "\rfoobar");
  SG_CHECK_EQUAL(strutils::stripTrailingNewlines("\rfoobar\r"), "\rfoobar");
  SG_CHECK_EQUAL(strutils::stripTrailingNewlines("\rfoobar"), "\rfoobar");
  SG_CHECK_EQUAL(strutils::stripTrailingNewlines(""), "");
}

void test_stripTrailingNewlines_inplace()
{
  string s = "\r\n\r\rfoo\n\r\rbar\n\r\n\r\r\n\r\r";
  strutils::stripTrailingNewlines_inplace(s);
  SG_CHECK_EQUAL(s, "\r\n\r\rfoo\n\r\rbar");

  s = "\rfoobar\r";
  strutils::stripTrailingNewlines_inplace(s);
  SG_CHECK_EQUAL(s, "\rfoobar");

  s = "\rfoobar";
  strutils::stripTrailingNewlines_inplace(s);
  SG_CHECK_EQUAL(s, "\rfoobar");

  s = "";
  strutils::stripTrailingNewlines_inplace(s);
  SG_CHECK_EQUAL(s, "");
}

void test_starts_with()
{
  SG_VERIFY(strutils::starts_with("banana", "ban"));
  SG_VERIFY(!strutils::starts_with("abanana", "ban"));
  // Pass - string starts with itself
  SG_VERIFY(strutils::starts_with("banana", "banana"));
  // Fail - original string is prefix of
  SG_VERIFY(!strutils::starts_with("ban", "banana"));
}

void test_ends_with()
{
  SG_VERIFY(strutils::ends_with("banana", "ana"));
  SG_VERIFY(strutils::ends_with("foo.text", ".text"));
  SG_VERIFY(!strutils::ends_with("foo.text", ".html"));
}

void test_simplify()
{
  SG_CHECK_EQUAL(strutils::simplify("\ta\t b  \nc\n\r \r\n"), "a b c");
  SG_CHECK_EQUAL(strutils::simplify("The quick  - brown dog!"),
                 "The quick - brown dog!");
  SG_CHECK_EQUAL(strutils::simplify("\r\n  \r\n   \t  \r"), "");
}

void test_to_int()
{
  SG_CHECK_EQUAL(strutils::to_int("999"), 999);
  SG_CHECK_EQUAL(strutils::to_int("0000000"), 0);
  SG_CHECK_EQUAL(strutils::to_int("-10000"), -10000);
}

// Auxiliary function for test_readNonNegativeInt()
void aux_readNonNegativeInt_setUpOStringStream(std::ostringstream& oss, int base)
{
  switch (base) {
  case 10:
    oss << std::dec;
    break;
  case 16:
    oss << std::hex;
    break;
  default:
    SG_TEST_FAIL("unsupported value for 'base': " + std::to_string(base));
  }
}

// Auxiliary function for test_readNonNegativeInt(): round-trip conversion for
// the given number of values below and up to std::numeric_limits<T>::max().
template<typename T, int BASE>
void aux_readNonNegativeInt_testValuesCloseToMax(T nbValues)
{
  std::ostringstream oss;

  assert(0 <= nbValues && nbValues <= std::numeric_limits<T>::max());
  aux_readNonNegativeInt_setUpOStringStream(oss, BASE);

  for (T i = std::numeric_limits<T>::max() - nbValues;
       i < std::numeric_limits<T>::max(); i++) {
    T valueToTest = i + 1;
    T roundTripResult;
    bool gotException = false;

    oss.str("");
    // The cast is only useful when T is a char type
    oss << static_cast<uintmax_t>(valueToTest);

    try {
      roundTripResult = strutils::readNonNegativeInt<T, BASE>(oss.str());
    } catch (const sg_range_exception&) {
      gotException = true;
    }

    SG_VERIFY(!gotException);
    SG_CHECK_EQUAL(roundTripResult, valueToTest);
  }
}

// Auxiliary class for test_readNonNegativeInt(): test that we do get an
// exception when trying to convert the smallest, positive out-of-range value
// for type T.
template<typename T, int BASE>
class ReadNonNegativeInt_JustOutOfRangeTester {
public:
  ReadNonNegativeInt_JustOutOfRangeTester()
  { }

  // Run the test
  void run()
  {
    std::ostringstream oss;
    aux_readNonNegativeInt_setUpOStringStream(oss, BASE);
    oss << 1 + static_cast<uintmax_t>(std::numeric_limits<T>::max());
    bool gotException = false;

    try {
      strutils::readNonNegativeInt<T, BASE>(oss.str());
    } catch (const sg_range_exception&) {
      gotException = true;
    }

    SG_VERIFY(gotException);
  }
};

class ReadNonNegativeInt_DummyTester {
public:
  ReadNonNegativeInt_DummyTester()
  { }

  void run()
  { }
};

// We use this helper class to automatically determine for which types
// ReadNonNegativeInt_JustOutOfRangeTester::run() can be run.
template<typename T, int BASE>
class AuxReadNonNegativeInt_JustOutOfRange_Helper
{
  typedef typename std::make_unsigned<T>::type uT;

  // Define TestRunner to be either
  //
  //   ReadNonNegativeInt_JustOutOfRangeTester<T, BASE>
  //
  // or
  //
  //   ReadNonNegativeInt_DummyTester
  //
  // depending on whether 1 + std::numeric_limits<T>::max() can be
  // represented by uintmax_t.
  typedef typename std::conditional<
    static_cast<uT>(std::numeric_limits<T>::max()) <
    std::numeric_limits<uintmax_t>::max(),
    ReadNonNegativeInt_JustOutOfRangeTester<T, BASE>,
    ReadNonNegativeInt_DummyTester >::type TestRunner;

public:
  AuxReadNonNegativeInt_JustOutOfRange_Helper()
  { };

  void test()
  {
    TestRunner().run();
  }
};

void test_readNonNegativeInt()
{
// In order to save some bytes for the SimGearCore library[*], we only
// instantiated a small number of variants of readNonNegativeInt() in
// strutils.cxx. This is why many tests are disabled with '#if 0' below. Of
// course, more variants can be enabled when they are needed.
//
// [*] See measures in strutils.cxx before the template instantiations.

#if 0
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<short>("0")), 0);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<short>("23")), 23);
#endif

  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int>("0")), 0);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int>("00000000")), 0);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int>("12345")), 12345);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int, 10>("12345")), 12345);

#if 0
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int, 16>("ff")), 0xff);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int, 16>("a5E9")), 0xa5e9);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<unsigned long, 16>("0cda")),
                 0x0cda);

  SG_CHECK_EQUAL((strutils::readNonNegativeInt<uint16_t, 10>("65535")), 0xffff);
  SG_CHECK_EQUAL(
    (strutils::readNonNegativeInt<uint16_t, 10>("00000000000000000000065535")),
    0xffff);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<uint16_t, 16>("ffff")), 0xffff);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int16_t, 10>("32767")), 0x7fff);
  SG_CHECK_EQUAL((strutils::readNonNegativeInt<int16_t, 16>("7fff")), 0x7fff);
#endif

  // Nothing special about the values :)
  SG_CHECK_EQUAL_NOSTREAM((typeid(strutils::readNonNegativeInt<int, 10>("72"))),
                          typeid(int(12)));
#if 0
  SG_CHECK_EQUAL_NOSTREAM((typeid(strutils::readNonNegativeInt<long, 10>("72"))),
                          typeid(12L));
  SG_CHECK_EQUAL_NOSTREAM(
    (typeid(strutils::readNonNegativeInt<long long, 10>("72"))),
    typeid(12LL));
#endif

  {
    bool gotException = false;
    try {
      strutils::readNonNegativeInt<int>("");   // empty string: illegal
    } catch (const sg_format_exception&) {
      gotException = true;
    }
    SG_VERIFY(gotException);
  }

  {
    bool gotException = false;
    try {
      strutils::readNonNegativeInt<int>("-1"); // non-digit character: illegal
    } catch (const sg_format_exception&) {
      gotException = true;
    }
    SG_VERIFY(gotException);
  }

  {
    bool gotException = false;
    try {
      strutils::readNonNegativeInt<int>("+1"); // non-digit character: illegal
    } catch (const sg_format_exception&) {
      gotException = true;
    }
    SG_VERIFY(gotException);
  }

  {
    bool gotException = false;
    try {
      strutils::readNonNegativeInt<int>("858efe"); // trailing garbage: illegal
    } catch (const sg_format_exception&) {
      gotException = true;
    }
    SG_VERIFY(gotException);
  }

#if 0
  {
    bool gotException = false;
    try {
      strutils::readNonNegativeInt<int, 16>("858g5k"); // ditto for base 16
    } catch (const sg_format_exception&) {
      gotException = true;
    }
    SG_VERIFY(gotException);
  }
#endif

  {
    bool gotException = false;
    try {
      strutils::readNonNegativeInt<int>("  858"); // leading whitespace/garbage:
    } catch (const sg_format_exception&) {        // illegal too
      gotException = true;
    }
    SG_VERIFY(gotException);
  }

  // Try to read a value that is 1 unit too large for the type. Check that it
  // raises an sg_range_exception in each case.
#if 0
  AuxReadNonNegativeInt_JustOutOfRange_Helper<signed char, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<signed char, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned char, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned char, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<short, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<short, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned short, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned short, 16>().test();
#endif

  AuxReadNonNegativeInt_JustOutOfRange_Helper<int, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned int, 10>().test();

#if 0
  AuxReadNonNegativeInt_JustOutOfRange_Helper<int, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned int, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<long, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<long, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned long, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned long, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<long long, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<long long, 16>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned long long, 10>().test();
  AuxReadNonNegativeInt_JustOutOfRange_Helper<unsigned long long, 16>().test();
#endif

  // Round trip tests with large values, including the largest value that can
  // be represented by the type, in each case.
  //
  // Can be casted as any of the following types
  constexpr int nbValues = 5000;
#if 0
  aux_readNonNegativeInt_testValuesCloseToMax<signed char, 10>(127);
  aux_readNonNegativeInt_testValuesCloseToMax<signed char, 16>(127);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned char, 10>(127);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned char, 16>(127);
  aux_readNonNegativeInt_testValuesCloseToMax<short, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<short, 16>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned short, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned short, 16>(nbValues);
#endif

  aux_readNonNegativeInt_testValuesCloseToMax<int, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned int, 10>(nbValues);

#if 0
  aux_readNonNegativeInt_testValuesCloseToMax<int, 16>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned int, 16>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<long, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<long, 16>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned long, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned long, 16>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<long long, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<long long, 16>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned long long, 10>(nbValues);
  aux_readNonNegativeInt_testValuesCloseToMax<unsigned long long, 16>(nbValues);
#endif
}

void test_split()
{
  string_list l = strutils::split("zero one two three four five");
  SG_CHECK_EQUAL(l[2], "two");
  SG_CHECK_EQUAL(l[5], "five");
  SG_CHECK_EQUAL(l.size(), 6);

  string j = strutils::join(l, "&");
  SG_CHECK_EQUAL(j, "zero&one&two&three&four&five");

  l = strutils::split("alpha:beta:gamma:delta", ":", 2);
  SG_CHECK_EQUAL(l.size(), 3);
  SG_CHECK_EQUAL(l[0], "alpha");
  SG_CHECK_EQUAL(l[1], "beta");
  SG_CHECK_EQUAL(l[2], "gamma:delta");

  l = strutils::split("", ",");
  SG_CHECK_EQUAL(l.size(), 1);
  SG_CHECK_EQUAL(l[0], "");

  l = strutils::split(",", ",");
  SG_CHECK_EQUAL(l.size(), 2);
  SG_CHECK_EQUAL(l[0], "");
  SG_CHECK_EQUAL(l[1], "");

  l = strutils::split(",,", ",");
  SG_CHECK_EQUAL(l.size(), 3);
  SG_CHECK_EQUAL(l[0], "");
  SG_CHECK_EQUAL(l[1], "");
  SG_CHECK_EQUAL(l[2], "");

  l = strutils::split("  ", ",");
  SG_CHECK_EQUAL(l.size(), 1);
  SG_CHECK_EQUAL(l[0], "  ");


    const char* testCases[] = {
        "alpha,bravo, charlie\tdelta\n\recho,    \t\r\nfoxtrot golf,\n\t\n \n",
        "  alpha  bravo \t charlie,,,delta    echo\nfoxtrot\rgolf"
    };

    for (const char* data: testCases) {
        l = strutils::split_on_any_of(data, "\n\t\r ,");
        SG_CHECK_EQUAL(l.size(), 7);
        SG_CHECK_EQUAL(l[0], "alpha");
        SG_CHECK_EQUAL(l[1], "bravo");
        SG_CHECK_EQUAL(l[2], "charlie");
        SG_CHECK_EQUAL(l[3], "delta");
        SG_CHECK_EQUAL(l[4], "echo");
        SG_CHECK_EQUAL(l[5], "foxtrot");
        SG_CHECK_EQUAL(l[6], "golf");
    }
}

void test_escape()
{
  SG_CHECK_EQUAL(strutils::escape(""), "");
  SG_CHECK_EQUAL(strutils::escape("\\"), "\\\\");
  SG_CHECK_EQUAL(strutils::escape("\""), "\\\"");
  SG_CHECK_EQUAL(strutils::escape("\\n"), "\\\\n");
  SG_CHECK_EQUAL(strutils::escape("n\\"), "n\\\\");
  SG_CHECK_EQUAL(strutils::escape(" ab\nc \\def\t\r \\ ghi\\"),
                 " ab\\nc \\\\def\\t\\r \\\\ ghi\\\\");
  // U+0152 is LATIN CAPITAL LIGATURE OE. The last word is Egg translated in
  // French and encoded in UTF-8 ('Œuf' if you can read UTF-8).
  SG_CHECK_EQUAL(strutils::escape("Un \"Bel\" '\u0152uf'"),
                 "Un \\\"Bel\\\" '\\305\\222uf'");
  SG_CHECK_EQUAL(strutils::escape("\a\b\f\n\r\t\v"),
                 "\\a\\b\\f\\n\\r\\t\\v");

  // Test with non-printable characters
  //
  // - 'prefix' is an std::string that *contains* a NUL character.
  // - \012 is \n (LINE FEED).
  // - \037 (\x1F) is the last non-printable ASCII character before \040 (\x20),
  //   which is the space.
  // - \176 (\x7E) is '~', the last printable ASCII character.
  // - \377 is \xFF. Higher char values (> 255) are not faithfully encoded by
  //   strutils::escape(): only the lowest 8 bits are used; higher-order bits
  //   are ignored (for people who use chars with more than 8 bits...).
  const string prefix = string("abc") + '\000';
  SG_CHECK_EQUAL(strutils::escape(prefix +
                                  "\003def\012\037\040\176\177\376\377"),
                 "abc\\000\\003def\\n\\037 ~\\177\\376\\377");

  SG_CHECK_EQUAL(strutils::escape(" \n\tAOa"), " \\n\\tAOa");
}

void test_unescape()
{
  SG_CHECK_EQUAL(strutils::unescape("\\ \\n\\t\\x41\\117a"), " \n\tAOa");
  // Two chars: '\033' (ESC) followed by '2'
  SG_CHECK_EQUAL(strutils::unescape("\\0332"), "\0332");
  // Hex escapes have no length limit and terminate at the first character
  // that is not a valid hexadecimal digit.
  SG_CHECK_EQUAL(strutils::unescape("\\x00020|"), " |");
  SG_CHECK_EQUAL(strutils::unescape("\\xA"), "\n");
  SG_CHECK_EQUAL(strutils::unescape("\\xA-"), "\n-");
}

void aux_escapeAndUnescapeRoundTripTest(const string& testString)
{
  SG_CHECK_EQUAL(strutils::unescape(strutils::escape(testString)), testString);
}

void test_escapeAndUnescapeRoundTrips()
{
  // "\0332" contains two chars: '\033' (ESC) followed by '2'.
  // Ditto for "\0402": it's a space ('\040') followed by a '2'.
  vector<string> stringsToTest(
    {"", "\\", "\n", "\\\\", "\"\'\?\t\rAG\v\a \b\f\\", "\x23\xf8",
     "\0332", "\0402", "\u00e0", "\U000000E9"});

  const string withBinary = (string("abc") + '\000' +
                             "\003def\012\037\040\176\177\376\377");
  stringsToTest.push_back(std::move(withBinary));

  for (const string& s: stringsToTest) {
    aux_escapeAndUnescapeRoundTripTest(s);
  }
}

void test_compare_versions()
{
  SG_CHECK_LT(strutils::compare_versions("1.0.12", "1.1"), 0);
  SG_CHECK_GT(strutils::compare_versions("1.1", "1.0.12"), 0);
  SG_CHECK_EQUAL(strutils::compare_versions("10.6.7", "10.6.7"), 0);
  SG_CHECK_LT(strutils::compare_versions("2.0", "2.0.99"), 0);
  SG_CHECK_EQUAL(strutils::compare_versions("99", "99"), 0);
  SG_CHECK_GT(strutils::compare_versions("99", "98"), 0);

  // Since we compare numerically, leading zeros shouldn't matter
  SG_CHECK_EQUAL(strutils::compare_versions("0.06.7", "0.6.07"), 0);


  SG_CHECK_EQUAL(strutils::compare_versions("10.6.7", "10.6.8", 2), 0);
  SG_CHECK_GT(strutils::compare_versions("10.7.7", "10.6.8", 2), 0);
  SG_CHECK_EQUAL(strutils::compare_versions("10.8.7", "10.6.8", 1), 0);
}

void test_md5_hex()
{
  // hex encoding
  unsigned char raw_data[] = {0x0f, 0x1a, 0xbc, 0xd2, 0xe3, 0x45, 0x67, 0x89};
  const string& hex_data =
    strutils::encodeHex(raw_data, sizeof(raw_data)/sizeof(raw_data[0]));
  SG_CHECK_EQUAL(hex_data, "0f1abcd2e3456789");
  SG_CHECK_EQUAL(strutils::encodeHex("abcde"), "6162636465");

  // md5
  SG_CHECK_EQUAL(strutils::md5("test"), "098f6bcd4621d373cade4e832627b4f6");
}

void test_propPathMatch()
{
    const char* testTemplate1 = "/sim[*]/views[*]/render";
    SG_VERIFY(strutils::matchPropPathToTemplate("/sim[0]/views[50]/render-buildings[0]", testTemplate1));
    SG_VERIFY(strutils::matchPropPathToTemplate("/sim[1]/views[0]/rendering-enabled", testTemplate1));

    SG_VERIFY(!strutils::matchPropPathToTemplate("/sim[0]/views[50]/something-else", testTemplate1));
    SG_VERIFY(!strutils::matchPropPathToTemplate("/sim[0]/gui[0]/wibble", testTemplate1));

    // test explicit index matching
    const char* testTemplate2 = "/view[5]/*";
    SG_VERIFY(!strutils::matchPropPathToTemplate("/view[2]/render-buildings[0]", testTemplate2));
    SG_VERIFY(!strutils::matchPropPathToTemplate("/sim[1]/foo", testTemplate2));
    SG_VERIFY(!strutils::matchPropPathToTemplate("/view[50]/foo", testTemplate2));
    SG_VERIFY(!strutils::matchPropPathToTemplate("/view[55]/foo", testTemplate2));

    SG_VERIFY(strutils::matchPropPathToTemplate("/view[5]/foo", testTemplate2));
    SG_VERIFY(strutils::matchPropPathToTemplate("/view[5]/child[3]/bar", testTemplate2));


    const char* testTemplate3 = "/*[*]/fdm*[*]/aero*";

    SG_VERIFY(strutils::matchPropPathToTemplate("/position[2]/fdm-jsb[0]/aerodynamic", testTemplate3));
    SG_VERIFY(!strutils::matchPropPathToTemplate("/position[2]/foo[0]/aerodynamic", testTemplate3));
}

void test_error_string()
{
#if defined(_WIN32)
  _set_errno(0);
#else
  errno = 0;
#endif

  std::ifstream f("/\\/non-existent/file/a8f7bz97-3ffe-4f5b-b8db-38ccurJL-");

#if defined(_WIN32)
  errno_t saved_errno = errno;
#else
  int saved_errno = errno;
#endif

  SG_VERIFY(!f.is_open());
  SG_CHECK_NE(saved_errno, 0);
  SG_CHECK_GT(strutils::error_string(saved_errno).size(), 0);
}

void test_readTime()
{
    SG_CHECK_EQUAL_EP(strutils::readTime(""), 0.0);

    SG_CHECK_EQUAL_EP(strutils::readTime("11"), 11.0);
    SG_CHECK_EQUAL_EP(strutils::readTime("+11"), 11.0);
    SG_CHECK_EQUAL_EP(strutils::readTime("-11"), -11.0);
    
    SG_CHECK_EQUAL_EP(strutils::readTime("11:30"), 11.5);
    SG_CHECK_EQUAL_EP(strutils::readTime("+11:15"), 11.25);
    SG_CHECK_EQUAL_EP(strutils::readTime("-11:45"), -11.75);
    
    const double seconds = 1 / 3600.0;
    SG_CHECK_EQUAL_EP(strutils::readTime("11:30:00"), 11.5);
    SG_CHECK_EQUAL_EP(strutils::readTime("+11:15:05"), 11.25 + 5 * seconds);
    SG_CHECK_EQUAL_EP(strutils::readTime("-11:45:15"), -(11.75 + 15 * seconds));

    SG_CHECK_EQUAL_EP(strutils::readTime("0:0:0"), 0);
    
    SG_CHECK_EQUAL_EP(strutils::readTime("0:0:28"), 28 * seconds);
    SG_CHECK_EQUAL_EP(strutils::readTime("-0:0:28"), -28 * seconds);
}

void test_utf8Convert()
{
    // F, smiley emoticon, Maths summation symbol, section sign
    std::wstring a(L"\u0046\U0001F600\u2211\u00A7");
    
    
    std::string utf8A = strutils::convertWStringToUtf8(a);
    SG_VERIFY(utf8A == std::string("F\xF0\x9F\x98\x80\xE2\x88\x91\xC2\xA7"));
    
    
    std::wstring aRoundTrip = strutils::convertUtf8ToWString(utf8A);
    SG_VERIFY(a == aRoundTrip);
}

int main(int argc, char* argv[])
{
    test_strip();
    test_stripTrailingNewlines();
    test_stripTrailingNewlines_inplace();
    test_starts_with();
    test_ends_with();
    test_simplify();
    test_to_int();
    test_readNonNegativeInt();
    test_split();
    test_escape();
    test_unescape();
    test_escapeAndUnescapeRoundTrips();
    test_compare_versions();
    test_md5_hex();
    test_error_string();
    test_propPathMatch();
    test_readTime();
    test_utf8Convert();
    
    return EXIT_SUCCESS;
}
