/*
 * test_validation.cpp - Unit tests for the validation module.
 *
 * The C headers are included with extern "C" so that the C functions can be
 * called from the C++ test code with C linkage.
 */

extern "C" {
#include "casefile.h"
#include "validation.h"
}

#include <gtest/gtest.h>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

/* ------------------------------------------------------------------ */
/* Trimming and copying                                                */
/* ------------------------------------------------------------------ */

TEST(ValidationTrimTest, RemovesLeadingAndTrailingWhitespace)
{
    char text[64] = "   detective   ";

    validationTrim(text);

    EXPECT_STREQ(text, "detective");
}

TEST(ValidationTrimTest, LeavesInnerSpacesUntouched)
{
    char text[64] = "  north  gate  ";

    validationTrim(text);

    EXPECT_STREQ(text, "north  gate");
}

TEST(ValidationTrimTest, HandlesNullPointerSafely)
{
    validationTrim(nullptr);

    SUCCEED();
}

TEST(ValidationCopyFieldTest, TruncatesOverlongSourceSafely)
{
    char destination[8];

    validationCopyField(destination, sizeof(destination), "abcdefghijklmnop");

    EXPECT_STREQ(destination, "abcdefg");
    EXPECT_EQ(strlen(destination), 7U);
}

TEST(ValidationCopyFieldTest, CopiesExactFitWithTerminator)
{
    char destination[8];

    validationCopyField(destination, sizeof(destination), "abcdefg");

    EXPECT_STREQ(destination, "abcdefg");
}

TEST(ValidationCopyFieldTest, NullSourceProducesEmptyDestination)
{
    char destination[8] = "stale";

    validationCopyField(destination, sizeof(destination), nullptr);

    EXPECT_STREQ(destination, "");
}

TEST(ValidationCopyFieldTest, CopyingAFieldOntoItselfPreservesTheContent)
{
    /*
     * Regression test. Copying a field onto itself once emptied the field,
     * because the destination was cleared before the source was measured.
     * The managers rely on this behaviour when they normalise a record in
     * place.
     */
    char destination[16] = "CF-2026-0001";

    validationCopyField(destination, sizeof(destination), destination);

    EXPECT_STREQ(destination, "CF-2026-0001");
}

TEST(ValidationCopyFieldTest, SelfCopyOfAnExactlyFittingFieldIsUnchanged)
{
    char destination[8] = "abcdefg";

    validationCopyField(destination, sizeof(destination), destination);

    EXPECT_STREQ(destination, "abcdefg");
}

/* ------------------------------------------------------------------ */
/* Emptiness and printability                                          */
/* ------------------------------------------------------------------ */

TEST(ValidationIsNotEmptyTest, RejectsEmptyString)
{
    EXPECT_EQ(validationIsNotEmpty(""), 0);
}

TEST(ValidationIsNotEmptyTest, RejectsWhitespaceOnlyString)
{
    EXPECT_EQ(validationIsNotEmpty("   \t  "), 0);
}

TEST(ValidationIsNotEmptyTest, AcceptsStringWithContent)
{
    EXPECT_EQ(validationIsNotEmpty("evidence locker L-14"), 1);
}

TEST(ValidationIsNotEmptyTest, NullPointerIsTreatedAsEmpty)
{
    EXPECT_EQ(validationIsNotEmpty(nullptr), 0);
}

/* ------------------------------------------------------------------ */
/* Integer parsing                                                     */
/* ------------------------------------------------------------------ */

TEST(ValidationParseIntTest, ParsesPositiveValue)
{
    int value = -1;

    EXPECT_EQ(validationParseInt("42", &value), CF_SUCCESS);
    EXPECT_EQ(value, 42);
}

TEST(ValidationParseIntTest, ParsesNegativeValue)
{
    int value = 0;

    EXPECT_EQ(validationParseInt("-17", &value), CF_SUCCESS);
    EXPECT_EQ(value, -17);
}

TEST(ValidationParseIntTest, ParsesZero)
{
    int value = 99;

    EXPECT_EQ(validationParseInt("0", &value), CF_SUCCESS);
    EXPECT_EQ(value, 0);
}

TEST(ValidationParseIntTest, RejectsTrailingGarbage)
{
    int value = 0;

    EXPECT_EQ(validationParseInt("12abc", &value), CF_ERR_INVALID_FORMAT);
}

TEST(ValidationParseIntTest, RejectsEmptyString)
{
    int value = 0;

    EXPECT_EQ(validationParseInt("", &value), CF_ERR_INVALID_FORMAT);
}

TEST(ValidationParseIntTest, RejectsNonNumericText)
{
    int value = 0;

    EXPECT_EQ(validationParseInt("twelve", &value), CF_ERR_INVALID_FORMAT);
}

TEST(ValidationParseIntTest, RejectsNullResultPointer)
{
    EXPECT_EQ(validationParseInt("42", nullptr), CF_ERR_INVALID_ARGUMENT);
}

TEST(ValidationParseIntTest, RejectsValueAboveIntegerRange)
{
    int value = 0;

    EXPECT_EQ(validationParseInt("99999999999999", &value), CF_ERR_INVALID_FORMAT);
}

/* ------------------------------------------------------------------ */
/* Date validation                                                     */
/* ------------------------------------------------------------------ */

TEST(ValidationIsValidDateTest, AcceptsOrdinaryDate)
{
    EXPECT_EQ(validationIsValidDate("2026-01-14"), 1);
}

TEST(ValidationIsValidDateTest, AcceptsLeapDayInLeapYear)
{
    EXPECT_EQ(validationIsValidDate("2024-02-29"), 1);
}

TEST(ValidationIsValidDateTest, RejectsLeapDayInNonLeapYear)
{
    EXPECT_EQ(validationIsValidDate("2026-02-29"), 0);
}

TEST(ValidationIsValidDateTest, RejectsCenturyNonLeapYear)
{
    EXPECT_EQ(validationIsValidDate("1900-02-29"), 0);
}

TEST(ValidationIsValidDateTest, AcceptsCenturyLeapYear)
{
    EXPECT_EQ(validationIsValidDate("2000-02-29"), 1);
}

TEST(ValidationIsValidDateTest, RejectsMonthZero)
{
    EXPECT_EQ(validationIsValidDate("2026-00-14"), 0);
}

TEST(ValidationIsValidDateTest, RejectsMonthThirteen)
{
    EXPECT_EQ(validationIsValidDate("2026-13-01"), 0);
}

TEST(ValidationIsValidDateTest, RejectsDayZero)
{
    EXPECT_EQ(validationIsValidDate("2026-01-00"), 0);
}

TEST(ValidationIsValidDateTest, RejectsDayBeyondMonthLength)
{
    EXPECT_EQ(validationIsValidDate("2026-04-31"), 0);
}

TEST(ValidationIsValidDateTest, RejectsWrongSeparator)
{
    EXPECT_EQ(validationIsValidDate("2026/01/14"), 0);
}

TEST(ValidationIsValidDateTest, RejectsWrongLength)
{
    EXPECT_EQ(validationIsValidDate("2026-1-14"), 0);
}

TEST(ValidationIsValidDateTest, RejectsNullPointer)
{
    EXPECT_EQ(validationIsValidDate(nullptr), 0);
}

/* ------------------------------------------------------------------ */
/* Identifier validation                                                */
/* ------------------------------------------------------------------ */

TEST(ValidationIsValidIdentifierTest, AcceptsWellFormedIdentifier)
{
    EXPECT_EQ(validationIsValidIdentifier("CF-2026-0001"), 1);
}

TEST(ValidationIsValidIdentifierTest, RejectsLowercaseLetters)
{
    EXPECT_EQ(validationIsValidIdentifier("cf-2026-0001"), 0);
}

TEST(ValidationIsValidIdentifierTest, RejectsLeadingDigit)
{
    EXPECT_EQ(validationIsValidIdentifier("1CF-2026"), 0);
}

TEST(ValidationIsValidIdentifierTest, RejectsEmptyString)
{
    EXPECT_EQ(validationIsValidIdentifier(""), 0);
}

TEST(ValidationIsValidIdentifierTest, RejectsOverlongIdentifier)
{
    std::string tooLong(16, 'A');

    EXPECT_EQ(validationIsValidIdentifier(tooLong.c_str()), 0);
}

TEST(ValidationIsValidIdentifierTest, AcceptsMaximumPermittedLength)
{
    std::string maximumLength(15, 'A');

    EXPECT_EQ(validationIsValidIdentifier(maximumLength.c_str()), 1);
}

/* ------------------------------------------------------------------ */
/* Phone, age, height and shoe size                                    */
/* ------------------------------------------------------------------ */

TEST(ValidationIsValidPhoneTest, AcceptsTenDigitNumber)
{
    EXPECT_EQ(validationIsValidPhone("9820041155"), 1);
}

TEST(ValidationIsValidPhoneTest, AcceptsNumberWithPlusPrefix)
{
    EXPECT_EQ(validationIsValidPhone("+919820041155"), 1);
}

TEST(ValidationIsValidPhoneTest, RejectsTooFewDigits)
{
    EXPECT_EQ(validationIsValidPhone("12345"), 0);
}

TEST(ValidationIsValidPhoneTest, RejectsNonDigitCharacter)
{
    EXPECT_EQ(validationIsValidPhone("98200ABCDE"), 0);
}

TEST(ValidationIsValidAgeTest, AcceptsReasonableAge)
{
    EXPECT_EQ(validationIsValidAge(34), 1);
}

TEST(ValidationIsValidAgeTest, RejectsZeroAge)
{
    EXPECT_EQ(validationIsValidAge(0), 0);
}

TEST(ValidationIsValidAgeTest, RejectsUnrealisticAge)
{
    EXPECT_EQ(validationIsValidAge(500), 0);
}

TEST(ValidationIsValidHeightTest, AcceptsPlausibleHeight)
{
    EXPECT_EQ(validationIsValidHeight(178), 1);
}

TEST(ValidationIsValidHeightTest, RejectsImplausibleHeight)
{
    EXPECT_EQ(validationIsValidHeight(20), 0);
}

TEST(ValidationIsValidShoeSizeTest, AcceptsPlausibleShoeSize)
{
    EXPECT_EQ(validationIsValidShoeSize(42), 1);
}

TEST(ValidationIsValidShoeSizeTest, RejectsImplausibleShoeSize)
{
    EXPECT_EQ(validationIsValidShoeSize(5), 0);
}

/* ------------------------------------------------------------------ */
/* Enumeration parsing                                                 */
/* ------------------------------------------------------------------ */

TEST(ValidationParseCaseStatusTest, ParsesKnownStatus)
{
    CaseStatus_t status = CASE_STATUS_ARCHIVED;

    EXPECT_EQ(validationParseCaseStatus("under_investigation", &status), CF_SUCCESS);
    EXPECT_EQ(status, CASE_STATUS_UNDER_INVESTIGATION);
}

TEST(ValidationParseCaseStatusTest, ParsesIgnoringCaseAndWhitespace)
{
    CaseStatus_t status = CASE_STATUS_ARCHIVED;

    EXPECT_EQ(validationParseCaseStatus("  EVIDENCE_ANALYSIS  ", &status), CF_SUCCESS);
    EXPECT_EQ(status, CASE_STATUS_EVIDENCE_ANALYSIS);
}

TEST(ValidationParseCaseStatusTest, RejectsUnknownStatus)
{
    CaseStatus_t status = CASE_STATUS_OPEN;

    EXPECT_EQ(validationParseCaseStatus("nonsense", &status), CF_ERR_INVALID_ENUM);
}

TEST(ValidationParseCaseStatusTest, RejectsEmptyStatus)
{
    CaseStatus_t status = CASE_STATUS_OPEN;

    EXPECT_EQ(validationParseCaseStatus("   ", &status), CF_ERR_EMPTY_INPUT);
}

TEST(ValidationParseCrimeTypeTest, ParsesKnownCrimeType)
{
    CrimeType_t crimeType = CRIME_TYPE_OTHER;

    EXPECT_EQ(validationParseCrimeType("theft", &crimeType), CF_SUCCESS);
    EXPECT_EQ(crimeType, CRIME_TYPE_THEFT);
}

TEST(ValidationParseCrimeTypeTest, RejectsUnknownCrimeType)
{
    CrimeType_t crimeType = CRIME_TYPE_THEFT;

    EXPECT_EQ(validationParseCrimeType("piracy", &crimeType), CF_ERR_INVALID_ENUM);
}

TEST(ValidationParseAttributeKeyTest, ParsesKnownAttributeKey)
{
    AttributeKey_t key = ATTRIBUTE_COUNT;

    EXPECT_EQ(validationParseAttributeKey("shoe_size", &key), CF_SUCCESS);
    EXPECT_EQ(key, ATTRIBUTE_SHOE_SIZE);
}

TEST(ValidationParseAttributeKeyTest, RejectsUnknownAttributeKey)
{
    AttributeKey_t key = ATTRIBUTE_GENDER;

    EXPECT_EQ(validationParseAttributeKey("favourite_colour", &key), CF_ERR_INVALID_ENUM);
}

TEST(ValidationParseEvidenceCategoryTest, ParsesKnownCategory)
{
    EvidenceCategory_t category = EVIDENCE_CATEGORY_OTHER;

    EXPECT_EQ(validationParseEvidenceCategory("physical_trace", &category), CF_SUCCESS);
    EXPECT_EQ(category, EVIDENCE_CATEGORY_PHYSICAL_TRACE);
}

TEST(ValidationParseCustodyActionTest, ParsesKnownAction)
{
    CustodyAction_t action = CUSTODY_ACTION_COLLECTED;

    EXPECT_EQ(validationParseCustodyAction("transferred", &action), CF_SUCCESS);
    EXPECT_EQ(action, CUSTODY_ACTION_TRANSFERRED);
}

/* ------------------------------------------------------------------ */
/* Case-insensitive comparison and substring search                    */
/* ------------------------------------------------------------------ */

TEST(ValidationCompareIgnoreCaseTest, EqualIgnoringCase)
{
    EXPECT_EQ(validationCompareIgnoreCase("Black", "black"), 0);
}

TEST(ValidationCompareIgnoreCaseTest, DifferentValuesDiffer)
{
    EXPECT_NE(validationCompareIgnoreCase("black", "brown"), 0);
}

TEST(ValidationContainsIgnoreCaseTest, FindsSubstringIgnoringCase)
{
    EXPECT_EQ(validationContainsIgnoreCase("Fibre strand recovered", "fibre"), 1);
}

TEST(ValidationContainsIgnoreCaseTest, ReportsMissingSubstring)
{
    EXPECT_EQ(validationContainsIgnoreCase("Fibre strand recovered", "vehicle"), 0);
}

TEST(ValidationContainsIgnoreCaseTest, EmptyNeedleAlwaysMatches)
{
    EXPECT_EQ(validationContainsIgnoreCase("anything", ""), 1);
}

/* ------------------------------------------------------------------ */
/* Length checks                                                       */
/* ------------------------------------------------------------------ */

TEST(ValidationCheckLengthTest, AcceptsTextThatFits)
{
    EXPECT_EQ(validationCheckLength("short", 16), CF_SUCCESS);
}

TEST(ValidationCheckLengthTest, RejectsTextThatWouldFillTheWholeField)
{
    /* A field of 16 bytes holds 15 characters plus the terminator. */
    std::string fillsTheField(16, 'x');

    EXPECT_EQ(validationCheckLength(fillsTheField.c_str(), 16), CF_ERR_INVALID_LENGTH);
}

TEST(ValidationCheckLengthTest, AcceptsTextOneCharacterBelowMaximum)
{
    /* 15 characters fit exactly into a 16 byte field. */
    std::string justFits(15, 'x');

    EXPECT_EQ(validationCheckLength(justFits.c_str(), 16), CF_SUCCESS);
}

TEST(ValidationCheckLengthTest, RejectsNullText)
{
    EXPECT_EQ(validationCheckLength(nullptr, 16), CF_ERR_INVALID_ARGUMENT);
}

TEST(ValidationCheckLengthTest, RejectsZeroLengthLimit)
{
    EXPECT_EQ(validationCheckLength("text", 0), CF_ERR_INVALID_ARGUMENT);
}

/* ------------------------------------------------------------------ */
/* Status messages                                                      */
/* ------------------------------------------------------------------ */

TEST(ValidationStatusMessageTest, EveryStatusHasANonEmptyMessage)
{
    for (int status = CF_SUCCESS; status <= CF_ERR_INTERNAL; status++)
    {
        const char *message = validationStatusMessage(static_cast<CfStatus_t>(status));

        EXPECT_NE(message, nullptr);
        EXPECT_STRNE(message, "");
    }
}

TEST(ValidationStatusMessageTest, UnknownStatusFallsBackToGenericMessage)
{
    EXPECT_STRNE(validationStatusMessage(static_cast<CfStatus_t>(9999)), "");
}

/* ------------------------------------------------------------------ */
/* Line reading                                                        */
/* ------------------------------------------------------------------ */

/*
 * These tests read from standard input, which the test binary redirects.
 * A temporary file supplies the input so that each case is repeatable and
 * the file is removed again afterwards.
 */
class LineReadingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        /* Make sure the directory that holds the temporary input file exists. */
        std::system("mkdir -p /tmp/opencode_casefile_test");

        savedStdin = dup(STDIN_FILENO);
    }

    void TearDown() override
    {
        restoreInput();

        if (std::remove(inputPath.c_str()) != 0)
        {
            /* The file may not exist, which is harmless. */
        }
    }

    /*
     * Write the given content to the temporary input file and point standard
     * input at it.
     */
    void redirectInput(const std::string &content)
    {
        FILE *stream = std::fopen(inputPath.c_str(), "w");

        ASSERT_NE(stream, nullptr);
        std::fputs(content.c_str(), stream);
        std::fclose(stream);

        FILE *input = std::fopen(inputPath.c_str(), "r");
        ASSERT_NE(input, nullptr);

        ASSERT_NE(dup2(fileno(input), STDIN_FILENO), -1);
        std::fclose(input);
    }

    /*
     * Point standard input back at the original stream.
     */
    void restoreInput()
    {
        if (savedStdin >= 0)
        {
            dup2(savedStdin, STDIN_FILENO);
            close(savedStdin);
            savedStdin = -1;
        }
    }

    std::string inputPath = "/tmp/opencode_casefile_test/input.txt";
    int savedStdin = -1;
};

TEST_F(LineReadingTest, EachReadConsumesExactlyOneLine)
{
    /*
     * Regression test. The newline used to be stripped before the check for
     * an over-long line, which made every read discard the following line as
     * well. Two lines of input therefore produced only one result.
     */
    char first[64];
    char second[64];

    redirectInput("first line\nsecond line\n");

    ASSERT_EQ(validationReadLine(nullptr, first, sizeof(first)), CF_SUCCESS);
    EXPECT_STREQ(first, "first line");

    ASSERT_EQ(validationReadLine(nullptr, second, sizeof(second)), CF_SUCCESS);
    EXPECT_STREQ(second, "second line");

}

TEST_F(LineReadingTest, OverlongLineIsTruncatedAndItsRemainderIsDiscarded)
{
    char first[64];
    char second[64];
    std::string longLine(200, 'x');

    redirectInput(longLine + "\nnext line\n");

    ASSERT_EQ(validationReadLine(nullptr, first, sizeof(first)), CF_SUCCESS);
    EXPECT_EQ(std::string(first).size(), 63U);

    ASSERT_EQ(validationReadLine(nullptr, second, sizeof(second)), CF_SUCCESS);
    EXPECT_STREQ(second, "next line");

}

TEST_F(LineReadingTest, CarriageReturnIsRemoved)
{
    char buffer[64];

    redirectInput("windows line\r\n");

    ASSERT_EQ(validationReadLine(nullptr, buffer, sizeof(buffer)), CF_SUCCESS);
    EXPECT_STREQ(buffer, "windows line");

}

TEST_F(LineReadingTest, NullBufferIsRejected)
{
    EXPECT_EQ(validationReadLine(nullptr, nullptr, 0), CF_ERR_INVALID_ARGUMENT);
}

} // namespace
