/*
 * validation.c - Implementation of the reusable validation helpers.
 *
 * All functions here are side-effect free apart from validationReadLine,
 * which is the single place in the project that reads from standard input.
 */

#include "validation.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static int isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int daysInMonth(int year, int month)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12)
    {
        return 0;
    }
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    return days[month - 1];
}

static char toLowerChar(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return (char)(value - 'A' + 'a');
    }
    return value;
}

/*
 * Look up a name in a table of name/enumeration pairs.
 *
 * The comparison ignores case and surrounding whitespace so that the user may
 * type "hair" or "HAIR" interchangeably.
 */
static CfStatus_t lookupName(const char *text, const char *const *names, int count, int *index)
{
    char buffer[TITLE_LENGTH];
    int i;

    if (text == NULL || names == NULL || index == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    validationCopyField(buffer, sizeof(buffer), text);
    validationTrim(buffer);

    if (!validationIsNotEmpty(buffer))
    {
        return CF_ERR_EMPTY_INPUT;
    }

    for (i = 0; i < count; i++)
    {
        if (validationCompareIgnoreCase(buffer, names[i]) == 0)
        {
            *index = i;
            return CF_SUCCESS;
        }
    }
    return CF_ERR_INVALID_ENUM;
}

/* ------------------------------------------------------------------ */
/* Input handling                                                      */
/* ------------------------------------------------------------------ */

CfStatus_t validationReadLine(const char *prompt, char *buffer, size_t bufferSize)
{
    char discard[FILE_LINE_LENGTH];

    if (buffer == NULL || bufferSize == 0U)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    buffer[0] = '\0';

    if (prompt != NULL)
    {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buffer, (int)bufferSize, stdin) == NULL)
    {
        if (feof(stdin) != 0)
        {
            buffer[0] = '\0';
            return CF_ERR_FILE_READ;
        }
        return CF_ERR_FILE_READ;
    }

    /*
     * The presence of a newline must be checked before it is stripped: if the
     * line did not fit in the buffer, the remainder is still in the input
     * stream and has to be discarded so that the next read starts at a fresh
     * line.
     */
    if (strchr(buffer, '\n') == NULL && feof(stdin) == 0)
    {
        if (fgets(discard, (int)sizeof(discard), stdin) == NULL)
        {
            /* Nothing left to discard. */
        }
    }

    /* Remove the trailing newline, if any. */
    buffer[ strcspn(buffer, "\r\n") ] = '\0';

    return CF_SUCCESS;
}

void validationTrim(char *text)
{
    size_t start;
    size_t end;

    if (text == NULL)
    {
        return;
    }

    start = 0U;
    while (text[start] != '\0' && isspace((unsigned char)text[start]) != 0)
    {
        start++;
    }

    if (start > 0U)
    {
        memmove(text, text + start, strlen(text + start) + 1U);
    }

    end = strlen(text);
    while (end > 0U && isspace((unsigned char)text[end - 1U]) != 0)
    {
        end--;
        text[end] = '\0';
    }
}

void validationCopyField(char *destination, size_t destinationSize, const char *source)
{
    size_t length;

    if (destination == NULL || destinationSize == 0U)
    {
        return;
    }

    if (source == NULL)
    {
        destination[0] = '\0';
        return;
    }

    /*
     * Copying a field onto itself must not destroy the source. The length is
     * measured before anything is written, and the copy is skipped entirely in
     * this case.
     */
    if (source == destination)
    {
        length = strlen(destination);
        if (length >= destinationSize)
        {
            destination[ destinationSize - 1U ] = '\0';
        }
        return;
    }

    length = strlen(source);
    if (length >= destinationSize)
    {
        length = destinationSize - 1U;
    }
    memcpy(destination, source, length);
    destination[ length ] = '\0';
}

int validationIsNotEmpty(const char *text)
{
    size_t i;

    if (text == NULL)
    {
        return 0;
    }

    for (i = 0U; text[i] != '\0'; i++)
    {
        if (isspace((unsigned char)text[i]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int validationIsPrintable(const char *text)
{
    size_t i;

    if (text == NULL)
    {
        return 0;
    }

    for (i = 0U; text[i] != '\0'; i++)
    {
        if (isprint((unsigned char)text[i]) == 0)
        {
            return 0;
        }
    }
    return 1;
}

CfStatus_t validationCheckLength(const char *text, size_t maxFieldLength)
{
    if (text == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (maxFieldLength == 0U)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (strlen(text) >= maxFieldLength)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    return CF_SUCCESS;
}

int validationCompareIgnoreCase(const char *left, const char *right)
{
    size_t i = 0U;

    if (left == NULL || right == NULL)
    {
        return (left == right) ? 0 : 1;
    }

    while (left[i] != '\0' && right[i] != '\0')
    {
        char a = toLowerChar(left[i]);
        char b = toLowerChar(right[i]);

        if (a != b)
        {
            return (int)((unsigned char)a) - (int)((unsigned char)b);
        }
        i++;
    }

    return (int)((unsigned char)toLowerChar(left[i])) - (int)((unsigned char)toLowerChar(right[i]));
}

int validationContainsIgnoreCase(const char *haystack, const char *needle)
{
    size_t haystackLength;
    size_t needleLength;
    size_t i;

    if (haystack == NULL || needle == NULL)
    {
        return 0;
    }

    haystackLength = strlen(haystack);
    needleLength = strlen(needle);

    if (needleLength == 0U)
    {
        return 1;
    }
    if (needleLength > haystackLength)
    {
        return 0;
    }

    for (i = 0U; i <= (haystackLength - needleLength); i++)
    {
        size_t j;

        for (j = 0U; j < needleLength; j++)
        {
            if (toLowerChar(haystack[i + j]) != toLowerChar(needle[j]))
            {
                break;
            }
        }
        if (j == needleLength)
        {
            return 1;
        }
    }
    return 0;
}

CfStatus_t validationParseInt(const char *text, int *result)
{
    size_t index = 0U;
    int negative = 0;
    long value = 0L;

    if (text == NULL || result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    if (text[0] == '+' || text[0] == '-')
    {
        negative = (text[0] == '-') ? 1 : 0;
        index = 1U;
    }

    if (text[index] == '\0')
    {
        return CF_ERR_INVALID_FORMAT;
    }

    for (; text[index] != '\0'; index++)
    {
        if (isdigit((unsigned char)text[index]) == 0)
        {
            return CF_ERR_INVALID_FORMAT;
        }
        value = (value * 10L) + (long)(text[index] - '0');
        if (value > 2147483647L)
        {
            return CF_ERR_INVALID_FORMAT;
        }
    }

    *result = negative ? -(int)value : (int)value;
    return CF_SUCCESS;
}

CfStatus_t validationReadInt(const char *prompt, int minValue, int maxValue, int *result)
{
    char buffer[FILE_LINE_LENGTH];
    int attempts = 0;
    const int maxAttempts = 5;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (minValue > maxValue)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    while (attempts < maxAttempts)
    {
        int parsed = 0;

        attempts++;

        if (validationReadLine(prompt, buffer, sizeof(buffer)) != CF_SUCCESS)
        {
            printf("  ! Could not read input. Please try again.\n");
            continue;
        }

        validationTrim(buffer);

        if (!validationIsNotEmpty(buffer))
        {
            printf("  ! Input is empty. Enter a whole number between %d and %d.\n", minValue, maxValue);
            continue;
        }

        if (validationParseInt(buffer, &parsed) != CF_SUCCESS)
        {
            printf("  ! '%s' is not a valid whole number.\n", buffer);
            continue;
        }

        if (parsed < minValue || parsed > maxValue)
        {
            printf("  ! Value %d is out of range. Enter a number between %d and %d.\n", parsed, minValue, maxValue);
            continue;
        }

        *result = parsed;
        return CF_SUCCESS;
    }

    printf("  ! Too many invalid entries. Using the minimum value %d.\n", minValue);
    *result = minValue;
    return CF_ERR_INVALID_FORMAT;
}

/* ------------------------------------------------------------------ */
/* Field validation                                                    */
/* ------------------------------------------------------------------ */

int validationIsValidDate(const char *text)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int index;

    if (text == NULL || strlen(text) != 10U)
    {
        return 0;
    }

    for (index = 0; index < 10; index++)
    {
        char current = text[index];

        if (index == 4 || index == 7)
        {
            if (current != '-')
            {
                return 0;
            }
            continue;
        }
        if (isdigit((unsigned char)current) == 0)
        {
            return 0;
        }
    }

    year = (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 + (text[3] - '0');
    month = (text[5] - '0') * 10 + (text[6] - '0');
    day = (text[8] - '0') * 10 + (text[9] - '0');

    if (year < 1900 || year > 2200)
    {
        return 0;
    }
    if (month < 1 || month > 12)
    {
        return 0;
    }
    if (day < 1 || day > daysInMonth(year, month))
    {
        return 0;
    }
    return 1;
}

int validationIsValidIdentifier(const char *text)
{
    size_t index;
    size_t length;

    if (text == NULL)
    {
        return 0;
    }

    length = strlen(text);
    if (length == 0U || length >= ID_LENGTH)
    {
        return 0;
    }

    if (isalpha((unsigned char)text[0]) == 0)
    {
        return 0;
    }

    for (index = 0U; index < length; index++)
    {
        char current = text[index];

        if (isupper((unsigned char)current) == 0
            && isdigit((unsigned char)current) == 0
            && current != '-')
        {
            return 0;
        }
    }
    return 1;
}

int validationIsValidPhone(const char *text)
{
    size_t index = 0U;
    size_t length;

    if (text == NULL)
    {
        return 0;
    }

    length = strlen(text);
    if (length > 0U && text[0] == '+')
    {
        index = 1U;
    }

    length -= index;
    if (length < 7U || length > 15U)
    {
        return 0;
    }

    for (; index < strlen(text); index++)
    {
        if (isdigit((unsigned char)text[index]) == 0)
        {
            return 0;
        }
    }
    return 1;
}

int validationIsValidAge(int age)
{
    return (age >= 1 && age <= 120);
}

int validationIsValidHeight(int heightCm)
{
    return (heightCm >= 100 && heightCm <= 250);
}

int validationIsValidShoeSize(int shoeSize)
{
    return (shoeSize >= 30 && shoeSize <= 52);
}

/* ------------------------------------------------------------------ */
/* Enumeration conversion                                              */
/* ------------------------------------------------------------------ */

static const char *const caseStatusNames[ CASE_STATUS_COUNT ] = {
    "open", "under_investigation", "evidence_analysis", "closed", "archived"
};

static const char *const casePriorityNames[ CASE_PRIORITY_COUNT ] = {
    "low", "medium", "high", "critical"
};

static const char *const crimeTypeNames[ CRIME_TYPE_COUNT ] = {
    "homicide", "theft", "assault", "fraud", "vandalism",
    "cybercrime", "traffic_fatality", "arson", "other"
};

static const char *const suspectStatusNames[ SUSPECT_STATUS_COUNT ] = {
    "under_review", "person_of_interest", "primary", "ruled_out", "confirmed"
};

static const char *const evidenceCategoryNames[ EVIDENCE_CATEGORY_COUNT ] = {
    "biological", "chemical", "digital", "ballistic",
    "physical_trace", "documentary", "vehicle", "other"
};

static const char *const evidenceStatusNames[ EVIDENCE_STATUS_COUNT ] = {
    "collected", "in_transit", "under_analysis", "analysed", "reported", "released"
};

static const char *const custodyActionNames[ CUSTODY_ACTION_COUNT ] = {
    "collected", "transferred", "sealed", "analysed", "released"
};

static const char *const attributeKeyNames[ ATTRIBUTE_COUNT ] = {
    "gender", "height", "hair", "shoe_size", "blood_group", "vehicle"
};

CfStatus_t validationParseCaseStatus(const char *text, CaseStatus_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, caseStatusNames, (int)CASE_STATUS_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (CaseStatus_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseCasePriority(const char *text, CasePriority_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, casePriorityNames, (int)CASE_PRIORITY_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (CasePriority_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseCrimeType(const char *text, CrimeType_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, crimeTypeNames, (int)CRIME_TYPE_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (CrimeType_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseSuspectStatus(const char *text, SuspectStatus_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, suspectStatusNames, (int)SUSPECT_STATUS_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (SuspectStatus_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseEvidenceCategory(const char *text, EvidenceCategory_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, evidenceCategoryNames, (int)EVIDENCE_CATEGORY_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (EvidenceCategory_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseEvidenceStatus(const char *text, EvidenceStatus_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, evidenceStatusNames, (int)EVIDENCE_STATUS_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (EvidenceStatus_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseCustodyAction(const char *text, CustodyAction_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, custodyActionNames, (int)CUSTODY_ACTION_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (CustodyAction_t)index;
    return CF_SUCCESS;
}

CfStatus_t validationParseAttributeKey(const char *text, AttributeKey_t *result)
{
    int index = 0;

    if (result == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    {
        CfStatus_t status = lookupName(text, attributeKeyNames, (int)ATTRIBUTE_COUNT, &index);

        if (status != CF_SUCCESS)
        {
            return status;
        }
    }
    *result = (AttributeKey_t)index;
    return CF_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Display helpers                                                     */
/* ------------------------------------------------------------------ */

const char *validationCaseStatusName(CaseStatus_t status)
{
    if (status < 0 || status >= CASE_STATUS_COUNT)
    {
        return "unknown";
    }
    return caseStatusNames[ status ];
}

const char *validationCasePriorityName(CasePriority_t priority)
{
    if (priority < 0 || priority >= CASE_PRIORITY_COUNT)
    {
        return "unknown";
    }
    return casePriorityNames[ priority ];
}

const char *validationCrimeTypeName(CrimeType_t crimeType)
{
    if (crimeType < 0 || crimeType >= CRIME_TYPE_COUNT)
    {
        return "unknown";
    }
    return crimeTypeNames[ crimeType ];
}

const char *validationSuspectStatusName(SuspectStatus_t status)
{
    if (status < 0 || status >= SUSPECT_STATUS_COUNT)
    {
        return "unknown";
    }
    return suspectStatusNames[ status ];
}

const char *validationEvidenceCategoryName(EvidenceCategory_t category)
{
    if (category < 0 || category >= EVIDENCE_CATEGORY_COUNT)
    {
        return "unknown";
    }
    return evidenceCategoryNames[ category ];
}

const char *validationEvidenceStatusName(EvidenceStatus_t status)
{
    if (status < 0 || status >= EVIDENCE_STATUS_COUNT)
    {
        return "unknown";
    }
    return evidenceStatusNames[ status ];
}

const char *validationCustodyActionName(CustodyAction_t action)
{
    if (action < 0 || action >= CUSTODY_ACTION_COUNT)
    {
        return "unknown";
    }
    return custodyActionNames[ action ];
}

const char *validationAttributeKeyName(AttributeKey_t key)
{
    if (key < 0 || key >= ATTRIBUTE_COUNT)
    {
        return "unknown";
    }
    return attributeKeyNames[ key ];
}

const char *validationStatusMessage(CfStatus_t status)
{
    switch (status)
    {
        case CF_SUCCESS:
            return "Success.";
        case CF_ERR_NULL_POINTER:
            return "Internal error: a required pointer was NULL.";
        case CF_ERR_INVALID_ARGUMENT:
            return "Invalid argument supplied to the function.";
        case CF_ERR_INVALID_FORMAT:
            return "The value entered is not in the expected format.";
        case CF_ERR_INVALID_DATE:
            return "The date is not valid. Use the YYYY-MM-DD format.";
        case CF_ERR_INVALID_ENUM:
            return "The value entered is not one of the permitted choices.";
        case CF_ERR_INVALID_LENGTH:
            return "The text entered is too long.";
        case CF_ERR_EMPTY_INPUT:
            return "A value is required. Input cannot be left empty.";
        case CF_ERR_NOT_FOUND:
            return "No matching record was found.";
        case CF_ERR_DUPLICATE_ID:
            return "That identifier is already in use.";
        case CF_ERR_CAPACITY_FULL:
            return "The maximum number of records has been reached.";
        case CF_ERR_FILE_OPEN:
            return "The data file could not be opened.";
        case CF_ERR_FILE_READ:
            return "The data file could not be read.";
        case CF_ERR_FILE_WRITE:
            return "The data file could not be written.";
        case CF_ERR_CORRUPT_FILE:
            return "The data file contains content in an unrecognised format.";
        case CF_ERR_INSUFFICIENT_EVIDENCE:
            return "Insufficient evidence: the recorded evidence cannot support a score.";
        case CF_ERR_INTERNAL:
            return "An unexpected internal error occurred.";
        default:
            return "An unknown error occurred.";
    }
}
