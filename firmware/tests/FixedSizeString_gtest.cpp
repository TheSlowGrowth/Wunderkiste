#include <gtest/gtest.h>
#include "FixedSizeString.h"

TEST(FixedSizeString, a_basics)
{
    // basics

    // size
    static constexpr auto constexprSizeTest = FixedSizeStr<8>("abcd").size();
    EXPECT_EQ(constexprSizeTest, 4u);

    // truncation of size in ctor
    static constexpr auto constexprTruncationTest = FixedSizeStr<8>("abcdefghijklmnopqrstuvw").size();
    EXPECT_EQ(constexprTruncationTest, 8u);

    // maxSize
    static constexpr auto constexprMaxSizeTest = FixedSizeStr<8>("abcde").maxSize();
    EXPECT_EQ(constexprMaxSizeTest, 8u);

    // empty
    static constexpr auto constexprEmptyTest = FixedSizeStr<8>("").empty();
    EXPECT_TRUE(constexprEmptyTest);
    static constexpr auto constexprEmptyTest2 = FixedSizeStr<8>("a").empty();
    EXPECT_FALSE(constexprEmptyTest2);
}

TEST(FixedSizeString, b_equals)
{
    // compare with different capacity
    EXPECT_TRUE(FixedSizeStr<8>("abcd") == FixedSizeStr<16>("abcd"));
    EXPECT_FALSE(FixedSizeStr<8>("abcd") == FixedSizeStr<16>("wxyz"));
    EXPECT_TRUE(FixedSizeStr<13>("abcd") != FixedSizeStr<6>("efgh"));
    EXPECT_FALSE(FixedSizeStr<13>("abcd") != FixedSizeStr<6>("abcd"));
    EXPECT_TRUE(FixedSizeStr<8>("abcd") == "abcd");
    EXPECT_FALSE(FixedSizeStr<8>("abcd") == "wxyz");
    EXPECT_TRUE(FixedSizeStr<13>("abcd") != "efgh");
    EXPECT_FALSE(FixedSizeStr<8>("abcd") != "abcd");

    // case sensitive
    EXPECT_FALSE(FixedSizeStr<8>("abcd") == "Abcd");
    EXPECT_TRUE(FixedSizeStr<8>("abcd") != "Abcd");

    // is constexpr ?
    static constexpr auto constexprTestEq = FixedSizeStr<8>("abcd") == "abcd";
    EXPECT_TRUE(constexprTestEq);
    static constexpr auto constexprTestNeq = FixedSizeStr<8>("abcd") != "defg";
    EXPECT_TRUE(constexprTestNeq);
}

TEST(FixedSizeString, c_startsWith)
{
    // startsWith with different maxLength
    EXPECT_TRUE(FixedSizeStr<9>("abcdefg").startsWith(FixedSizeStr<23>("abcd")));
    EXPECT_TRUE(FixedSizeStr<9>("abcdefg").startsWith("abcd"));
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").startsWith(FixedSizeStr<23>("bcdef")));
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").startsWith("ghij"));
    // case sensitive !
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").startsWith("aBcD"));
    // is constexpr ?
    static constexpr auto constexprTest = FixedSizeStr<9>("abcdefg").startsWithIgnoringCase("abcd");
    EXPECT_TRUE(constexprTest);
}

TEST(FixedSizeString, d_startsWithIgnoringCase)
{
    // startsWithIgnoringCase with different maxLength
    EXPECT_TRUE(FixedSizeStr<9>("aBcDeFg").startsWithIgnoringCase(FixedSizeStr<23>("AbCd")));
    EXPECT_TRUE(FixedSizeStr<9>("aBcDeFg").startsWithIgnoringCase("ABcD"));
    EXPECT_FALSE(FixedSizeStr<9>("aBcDeFg").startsWithIgnoringCase(FixedSizeStr<23>("bCdEf")));
    EXPECT_FALSE(FixedSizeStr<9>("aBcDeFg").startsWithIgnoringCase("ghij"));
    // is constexpr ?
    static constexpr auto constexprTest = FixedSizeStr<9>("abcdefg").startsWithIgnoringCase("aBCd");
    EXPECT_TRUE(constexprTest);
}

TEST(FixedSizeString, e_endsWith)
{
    // endsWith with different maxLength
    EXPECT_TRUE(FixedSizeStr<9>("abcdefg").endsWith(FixedSizeStr<23>("defg")));
    EXPECT_TRUE(FixedSizeStr<9>("abcdefg").endsWith("defg"));
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").endsWith(FixedSizeStr<23>("abcd")));
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").endsWith("abcd"));
    // case sensitive !
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").endsWith("DefG"));
    // is constexpr ?
    static constexpr auto constexprTest = FixedSizeStr<9>("abcdefg").endsWith("defg");
    EXPECT_TRUE(constexprTest);
}

TEST(FixedSizeString, f_endsWithIgnoringCase)
{
    // endsWithIgnoringCase with different maxLength
    EXPECT_TRUE(FixedSizeStr<9>("abcdefg").endsWithIgnoringCase(FixedSizeStr<23>("DeFg")));
    EXPECT_TRUE(FixedSizeStr<9>("abcdefg").endsWithIgnoringCase("cdEfG"));
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").endsWithIgnoringCase(FixedSizeStr<23>("abcd")));
    EXPECT_FALSE(FixedSizeStr<9>("abcdefg").endsWithIgnoringCase("abcd"));
    // is constexpr ?
    static constexpr auto constexprTest = FixedSizeStr<9>("abcdefg").endsWithIgnoringCase("DeFg");
    EXPECT_TRUE(constexprTest);
}

TEST(FixedSizeString, g_clear)
{
    // clear
    auto str = FixedSizeStr<9>("abcdefg");
    EXPECT_FALSE(str.empty());
    str.clear();
    EXPECT_TRUE(str.empty());

    // clear should also work in a constexpr use case
    constexpr auto getEmptyStringFunc = []() {
        FixedSizeStr<9> str("abcdefg");
        str.clear();
        return str;
    };
    static constexpr auto constexprClearTest = getEmptyStringFunc().empty();
    EXPECT_TRUE(constexprClearTest);
}

TEST(FixedSizeString, h_reset)
{
    // reset
    auto str = FixedSizeStr<6>("abcdefg");
    str.reset("1234");
    EXPECT_STREQ(str, "1234");

    // respect max length
    str.reset("1234567890");
    EXPECT_STREQ(str, "123456");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<9> str("abcdefg");
        str.reset("1234");
        return str;
    };
    static constexpr auto constexprResetTest = getStringFunc();
    EXPECT_STREQ(constexprResetTest, "1234");
}

TEST(FixedSizeString, i_appendString)
{
    // append
    auto str = FixedSizeStr<9>("abcd");
    str.append("1234");
    EXPECT_STREQ(str, "abcd1234");

    // respect max length
    str.append("qwertz");
    EXPECT_STREQ(str, "abcd1234q");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<9> str("abcd");
        str.append("1234");
        return str;
    };
    static constexpr auto constexprAppendTest = getStringFunc();
    EXPECT_STREQ(constexprAppendTest, "abcd1234");
}

TEST(FixedSizeString, j_appendStringN)
{
    // append
    auto str = FixedSizeStr<9>("abcd");
    str.append("1234", 2);
    EXPECT_STREQ(str, "abcd12");

    // respect max length
    str.append("qwertz", 5);
    EXPECT_STREQ(str, "abcd12qwe");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<9> str("abcd");
        str.append("1234", 2);
        return str;
    };
    static constexpr auto constexprAppendTest = getStringFunc();
    EXPECT_STREQ(constexprAppendTest, "abcd12");
}

TEST(FixedSizeString, k_appendCharacter)
{
    // append
    auto str1 = FixedSizeStr<9>("abcd");
    str1.append('1');
    EXPECT_STREQ(str1, "abcd1");

    // respect max length (don't overwrite terminating zero!)
    auto str2 = FixedSizeStr<4>("abcd");
    str2.append('1');
    EXPECT_STREQ(str2, "abcd");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<9> str("abcd");
        str.append('1');
        return str;
    };
    static constexpr auto constexprAppendTest = getStringFunc();
    EXPECT_STREQ(constexprAppendTest, "abcd1");
}

TEST(FixedSizeString, l_removePrefix)
{
    // remove prefix
    auto str = FixedSizeStr<9>("abcd");
    str.removePrefix(2);
    EXPECT_STREQ(str, "cd");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<9> str("abcd");
        str.removePrefix(2);
        return str;
    };
    static constexpr auto constexprAppendTest = getStringFunc();
    EXPECT_STREQ(constexprAppendTest, "cd");
}

TEST(FixedSizeString, m_removeSuffix)
{
    // remove suffix
    auto str = FixedSizeStr<9>("abcd");
    str.removeSuffix(2);
    EXPECT_STREQ(str, "ab");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<9> str("abcd");
        str.removeSuffix(2);
        return str;
    };
    static constexpr auto constexprAppendTest = getStringFunc();
    EXPECT_STREQ(constexprAppendTest, "ab");
}

TEST(FixedSizeString, n_swap)
{
    // remove suffix
    auto str1 = FixedSizeStr<4>("12");
    auto str2 = FixedSizeStr<4>("34");
    str1.swap(str2);
    EXPECT_STREQ(str1, "34");
    EXPECT_STREQ(str2, "12");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        auto str1 = FixedSizeStr<4>("12");
        auto str2 = FixedSizeStr<4>("34");
        str1.swap(str2);
        return str1;
    };
    static constexpr auto constexprAppendTest = getStringFunc() == "34";
    EXPECT_TRUE(constexprAppendTest);
}

TEST(FixedSizeString, o_updateSize)
{
    // update size

    auto str = FixedSizeStr<4>("12");
    str.data()[2] = '3';
    str.data()[3] = 0;
    str.updateSize();
    EXPECT_EQ(str.size(), 3u);

    // write beyond string capacity
    str.data()[3] = '4';
    str.data()[4] = '5';
    // terminating zero must be restored
    str.updateSize();
    EXPECT_EQ(str.size(), 4u);
    EXPECT_EQ(str.data()[4], 0);

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        auto str = FixedSizeStr<4>("12");
        str.data()[2] = '3';
        str.data()[3] = 0;
        str.updateSize();
        return str;
    };
    static constexpr auto constexprUpdateLengthTest = getStringFunc().size();
    EXPECT_EQ(constexprUpdateLengthTest, 3u);
}

TEST(FixedSizeString, p_assignment)
{
    // assignment from different capacity string object, respecting capacity
    FixedSizeStr<4> str;
    str = FixedSizeStr<6>("abcdef");
    EXPECT_STREQ(str, "abcd");

    // assign from a shorter string
    str = FixedSizeStr<2>("xy");
    EXPECT_STREQ(str, "xy");

    // assignment from const char*, respecting capacity
    str = "123456";
    EXPECT_STREQ(str, "1234");

    // assign from a shorter string
    str = "ab";
    EXPECT_STREQ(str, "ab");

    // should also work in a constexpr use case
    constexpr auto getStringFunc1 = []() {
        FixedSizeStr<2> str;
        str = FixedSizeStr<4>("abc");
        return str;
    };
    constexpr auto getStringFunc2 = []() {
        FixedSizeStr<2> str;
        str = "123";
        return str;
    };
    static constexpr auto constexprAssignTest1 = getStringFunc1();
    static constexpr auto constexprAssignTest2 = getStringFunc2();
    EXPECT_STREQ(constexprAssignTest1, "ab");
    EXPECT_STREQ(constexprAssignTest2, "12");
}

TEST(FixedSizeString, q_resetAt)
{
    // reset at
    FixedSizeStr<5> str("abcd");
    str.resetAt("12", 1);
    EXPECT_STREQ(str, "a12d");

    // reset at, exceeding current length and capacity
    str.resetAt("uvwxyz", 3);
    EXPECT_STREQ(str, "a12uv");

    // should also work in a constexpr use case
    constexpr auto getStringFunc = []() {
        FixedSizeStr<5> str("abcd");
        str.resetAt("12", 1);
        return str;
    };
    static constexpr auto constexprResetAtTest = getStringFunc();
    EXPECT_STREQ(constexprResetAtTest, "a12d");
}

TEST(FixedSizeString, r_operatorGtLt)
{
    //  operator<(), operator<=(), operator>(), operator>=()

    // < / <= based on content
    EXPECT_TRUE(FixedSizeStr<5>("abc") < "abd");
    EXPECT_FALSE(FixedSizeStr<5>("abc") < "abb");
    EXPECT_TRUE(FixedSizeStr<5>("abc") <= "abd");
    EXPECT_TRUE(FixedSizeStr<5>("abc") <= "abc");

    // < / <= based on length alone
    EXPECT_TRUE(FixedSizeStr<5>("abc") < "abcd");
    EXPECT_FALSE(FixedSizeStr<5>("abc") < "abc");
    EXPECT_TRUE(FixedSizeStr<5>("abc") <= "abcd");
    EXPECT_TRUE(FixedSizeStr<5>("abc") <= "abc");

    // > / >= based on content
    EXPECT_TRUE(FixedSizeStr<5>("abc") > "abb");
    EXPECT_FALSE(FixedSizeStr<5>("abc") > "abd");
    EXPECT_TRUE(FixedSizeStr<5>("abc") >= "abb");
    EXPECT_TRUE(FixedSizeStr<5>("abc") >= "abc");

    // > / >= based on length alone
    EXPECT_TRUE(FixedSizeStr<5>("abcd") > "abc");
    EXPECT_FALSE(FixedSizeStr<5>("abc") > "abc");
    EXPECT_TRUE(FixedSizeStr<5>("abcd") >= "abc");
    EXPECT_TRUE(FixedSizeStr<5>("abc") >= "abc");

}