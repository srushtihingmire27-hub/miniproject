/*
 * investigation.c - Implementation of the rule-based evidence analysis.
 *
 * The matching rules implemented here are:
 *
 *   Rule 1  gender        - exact match, ignoring case              weight 15
 *   Rule 2  height        - suspect height inside the recorded band   weight 20
 *   Rule 3  hair          - exact match, ignoring case              weight 10
 *   Rule 4  shoe size     - suspect shoe size inside the recorded band weight 15
 *   Rule 5  blood group   - exact match, ignoring case              weight 20
 *   Rule 6  vehicle       - registration present in the suspect list weight 25
 *
 * A band value is written as "low-high", for example "175-185". When the band
 * cannot be parsed, the rule falls back to an exact comparison.
 *
 * The score is the sum of the weights of the matching attributes, capped at
 * MAX_SUSPICION_SCORE. Every attribute that was checked is recorded, whether
 * it matched or not, so the investigator can see what was examined.
 */

#include "investigation.h"
#include "case_manager.h"
#include "evidence_manager.h"
#include "suspect_manager.h"
#include "witness_manager.h"
#include "validation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/*
 * Parse a band of the form "low-high".
 *
 * @param text  Band text (must not be NULL).
 * @param low   Receives the lower bound.
 * @param high  Receives the upper bound.
 * @return 1 when the band was parsed, 0 otherwise.
 */
static int parseBand(const char *text, int *low, int *high)
{
    const char *dash;
    char lowText[16];
    char highText[16];
    size_t lowLength;

    if (text == NULL || low == NULL || high == NULL)
    {
        return 0;
    }

    dash = strchr(text, '-');
    if (dash == NULL)
    {
        return 0;
    }

    lowLength = (size_t)(dash - text);
    if (lowLength == 0U || lowLength >= sizeof(lowText))
    {
        return 0;
    }

    memcpy(lowText, text, lowLength);
    lowText[ lowLength ] = '\0';

    if (strlen(dash + 1) >= sizeof(highText))
    {
        return 0;
    }
    validationCopyField(highText, sizeof(highText), dash + 1);
    validationTrim(lowText);
    validationTrim(highText);

    if (validationParseInt(lowText, low) != CF_SUCCESS)
    {
        return 0;
    }
    if (validationParseInt(highText, high) != CF_SUCCESS)
    {
        return 0;
    }
    return 1;
}

/*
 * Return the suspect attribute that corresponds to an attribute key.
 *
 * Only the textual attributes are returned here. The numeric attributes
 * (height and shoe size) and the vehicle list are handled separately because
 * they need a range comparison or a list search.
 *
 * @param suspect Suspect whose profile is read (must not be NULL).
 * @param key     Attribute key to look up.
 * @return Profile string describing the suspect value, or NULL when the
 *         suspect has no value recorded for that key.
 */
static const char *suspectAttributeValue(const Suspect_t *suspect, AttributeKey_t key)
{
    if (suspect == NULL)
    {
        return NULL;
    }

    switch (key)
    {
        case ATTRIBUTE_GENDER:
            return (validationIsNotEmpty(suspect->physical.gender) != 0)
                   ? suspect->physical.gender : NULL;
        case ATTRIBUTE_HAIR:
            return (validationIsNotEmpty(suspect->physical.hairColour) != 0)
                   ? suspect->physical.hairColour : NULL;
        case ATTRIBUTE_BLOOD_GROUP:
            return (validationIsNotEmpty(suspect->physical.bloodGroup) != 0)
                   ? suspect->physical.bloodGroup : NULL;
        default:
            return NULL;
    }
}

int investigation_weightForKey(AttributeKey_t key)
{
    switch (key)
    {
        case ATTRIBUTE_GENDER:
            return WEIGHT_GENDER;
        case ATTRIBUTE_HEIGHT:
            return WEIGHT_HEIGHT;
        case ATTRIBUTE_HAIR:
            return WEIGHT_HAIR;
        case ATTRIBUTE_SHOE_SIZE:
            return WEIGHT_SHOE_SIZE;
        case ATTRIBUTE_BLOOD_GROUP:
            return WEIGHT_BLOOD_GROUP;
        case ATTRIBUTE_VEHICLE:
            return WEIGHT_VEHICLE;
        default:
            return 0;
    }
}

int investigation_compareAttribute(AttributeKey_t key, const char *evidenceValue,
                                   const char *suspectValue, int *matched)
{
    if (matched == NULL)
    {
        return 0;
    }

    *matched = 0;

    if (evidenceValue == NULL || suspectValue == NULL)
    {
        return 0;
    }

    switch (key)
    {
        case ATTRIBUTE_GENDER:
        case ATTRIBUTE_HAIR:
        case ATTRIBUTE_BLOOD_GROUP:
            if (validationCompareIgnoreCase(evidenceValue, suspectValue) == 0)
            {
                *matched = 1;
                return investigation_weightForKey(key);
            }
            return 0;

        case ATTRIBUTE_HEIGHT:
        {
            int low = 0;
            int high = 0;
            int height = 0;

            if (parseBand(evidenceValue, &low, &high) == 0)
            {
                if (validationCompareIgnoreCase(evidenceValue, suspectValue) == 0)
                {
                    *matched = 1;
                    return investigation_weightForKey(key);
                }
                return 0;
            }

            if (validationParseInt(suspectValue, &height) != CF_SUCCESS)
            {
                return 0;
            }
            if (height >= low && height <= high)
            {
                *matched = 1;
                return investigation_weightForKey(key);
            }
            return 0;
        }

        case ATTRIBUTE_SHOE_SIZE:
        {
            int low = 0;
            int high = 0;
            int shoe = 0;

            if (parseBand(evidenceValue, &low, &high) == 0)
            {
                if (validationCompareIgnoreCase(evidenceValue, suspectValue) == 0)
                {
                    *matched = 1;
                    return investigation_weightForKey(key);
                }
                return 0;
            }

            if (validationParseInt(suspectValue, &shoe) != CF_SUCCESS)
            {
                return 0;
            }
            if (shoe >= low && shoe <= high)
            {
                *matched = 1;
                return investigation_weightForKey(key);
            }
            return 0;
        }

        case ATTRIBUTE_VEHICLE:
            if (validationCompareIgnoreCase(evidenceValue, suspectValue) == 0)
            {
                *matched = 1;
                return investigation_weightForKey(key);
            }
            return 0;

        default:
            return 0;
    }
}

/*
 * Compare an evidence attribute with the physical profile of a suspect.
 *
 * @param suspect        Suspect to compare against (must not be NULL).
 * @param attribute      Attribute recorded against the evidence (must not be NULL).
 * @param suspectValueText Receives the textual form of the suspect value for
 *                        the report; it is empty when the suspect has no value
 *                        recorded for that key.
 * @param matched        Receives 1 when the attribute matches.
 * @return The weight contributed by a match, or 0 otherwise.
 */
static int compareWithProfile(const Suspect_t *suspect, const Attribute_t *attribute,
                               char *suspectValueText, int *matched)
{
    const char *profileValue = NULL;

    *matched = 0;
    suspectValueText[0] = '\0';

    if (suspect == NULL || attribute == NULL)
    {
        return 0;
    }

    switch (attribute->key)
    {
        case ATTRIBUTE_GENDER:
            profileValue = suspectAttributeValue(suspect, attribute->key);
            break;
        case ATTRIBUTE_HEIGHT:
            if (suspect->physical.heightCm >= 0)
            {
                snprintf(suspectValueText, ATTRIBUTE_VALUE_LENGTH, "%d", suspect->physical.heightCm);
                profileValue = suspectValueText;
            }
            break;
        case ATTRIBUTE_HAIR:
            profileValue = suspectAttributeValue(suspect, attribute->key);
            break;
        case ATTRIBUTE_SHOE_SIZE:
            if (suspect->physical.shoeSize >= 0)
            {
                snprintf(suspectValueText, ATTRIBUTE_VALUE_LENGTH, "%d", suspect->physical.shoeSize);
                profileValue = suspectValueText;
            }
            break;
        case ATTRIBUTE_BLOOD_GROUP:
            profileValue = suspectAttributeValue(suspect, attribute->key);
            break;
        case ATTRIBUTE_VEHICLE:
        {
            int vehicle;

            for (vehicle = 0; vehicle < suspect->physical.vehicleCount; vehicle++)
            {
                if (validationCompareIgnoreCase(suspect->physical.vehicles[ vehicle ],
                                                attribute->value) == 0)
                {
                    *matched = 1;
                    validationCopyField(suspectValueText, ATTRIBUTE_VALUE_LENGTH,
                                        suspect->physical.vehicles[ vehicle ]);
                    return investigation_weightForKey(attribute->key);
                }
            }
            return 0;
        }
        default:
            return 0;
    }

    if (profileValue == NULL)
    {
        return 0;
    }

    /* Keep a copy of the suspect value so that the report can show it. */
    validationCopyField(suspectValueText, ATTRIBUTE_VALUE_LENGTH, profileValue);

    return investigation_compareAttribute(attribute->key, attribute->value, profileValue, matched);
}

void investigation_rankSuspects(SuspectScore_t *scores, int count)
{
    int outer;
    int inner;

    if (scores == NULL || count < 2)
    {
        return;
    }

    /* Bubble sort by descending score; stable for equal scores. */
    for (outer = 0; outer < (count - 1); outer++)
    {
        for (inner = 0; inner < (count - 1 - outer); inner++)
        {
            if (scores[ inner ].score < scores[ inner + 1 ].score)
            {
                SuspectScore_t swap = scores[ inner ];

                scores[ inner ] = scores[ inner + 1 ];
                scores[ inner + 1 ] = swap;
            }
        }
    }
}

CfStatus_t investigation_analyseCase(const CasefileDb_t *db, const char *caseId,
                                      AnalysisResult_t *result)
{
    size_t caseIndex;
    size_t evidenceIndex;
    size_t suspectIndex;
    int comparable = 0;

    if (db == NULL || caseId == NULL || result == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    memset(result, 0, sizeof(*result));

    for (caseIndex = 0U; caseIndex < db->caseCount; caseIndex++)
    {
        if (strcmp(db->cases[ caseIndex ].caseId, caseId) == 0)
        {
            break;
        }
    }
    if (caseIndex >= db->caseCount)
    {
        return CF_ERR_NOT_FOUND;
    }

    /* Collect the suspects of the case. */
    for (suspectIndex = 0U; suspectIndex < db->suspectCount; suspectIndex++)
    {
        if (strcmp(db->suspects[ suspectIndex ].caseId, caseId) == 0)
        {
            if ((size_t)result->suspectCount >= (size_t)MAX_RANKED_SUSPECTS)
            {
                break;
            }

            validationCopyField(result->suspects[ result->suspectCount ].suspectId,
                                ID_LENGTH, db->suspects[ suspectIndex ].suspectId);
            validationCopyField(result->suspects[ result->suspectCount ].suspectName,
                                NAME_LENGTH, db->suspects[ suspectIndex ].fullName);
            result->suspectCount++;
        }
    }

    /* Count the evidence items that carry at least one comparable attribute. */
    for (evidenceIndex = 0U; evidenceIndex < db->evidenceCount; evidenceIndex++)
    {
        if (strcmp(db->evidence[ evidenceIndex ].caseId, caseId) != 0)
        {
            continue;
        }
        if (db->evidence[ evidenceIndex ].attributeCount > 0)
        {
            comparable++;
        }
    }

    result->comparableEvidence = comparable;
    result->evaluated = result->suspectCount;

    /*
     * Without suspects or without comparable evidence the engine must not
     * present a score: doing so would imply the evidence was weighed when it
     * was not.
     */
    if (result->suspectCount == 0 || comparable == 0)
    {
        return CF_ERR_INSUFFICIENT_EVIDENCE;
    }

    /* Score every suspect against every comparable attribute of the case. */
    for (suspectIndex = 0U; suspectIndex < db->suspectCount; suspectIndex++)
    {
        int target = -1;
        int slot;

        if (strcmp(db->suspects[ suspectIndex ].caseId, caseId) != 0)
        {
            continue;
        }

        for (slot = 0; slot < result->suspectCount; slot++)
        {
            if (strcmp(result->suspects[ slot ].suspectId,
                       db->suspects[ suspectIndex ].suspectId) == 0)
            {
                target = slot;
                break;
            }
        }
        if (target < 0)
        {
            continue;
        }

        for (evidenceIndex = 0U; evidenceIndex < db->evidenceCount; evidenceIndex++)
        {
            int attributeIndex;

            if (strcmp(db->evidence[ evidenceIndex ].caseId, caseId) != 0)
            {
                continue;
            }

            for (attributeIndex = 0;
                 attributeIndex < db->evidence[ evidenceIndex ].attributeCount;
                 attributeIndex++)
            {
                const Attribute_t *attribute = &db->evidence[ evidenceIndex ].attributes[ attributeIndex ];
                SuspectScore_t *score = &result->suspects[ target ];
                char suspectValueText[ATTRIBUTE_VALUE_LENGTH];
                int matched = 0;
                int weight;

                if (score->factorCount >= MAX_MATCH_FACTORS)
                {
                    continue;
                }

                weight = compareWithProfile(&db->suspects[ suspectIndex ], attribute,
                                            suspectValueText, &matched);

                score->checkedCount++;
                if (matched != 0)
                {
                    score->matchedCount++;
                    score->score += weight;
                }

                validationCopyField(score->factors[ score->factorCount ].evidenceId,
                                    ID_LENGTH, db->evidence[ evidenceIndex ].evidenceId);
                score->factors[ score->factorCount ].key = attribute->key;
                validationCopyField(score->factors[ score->factorCount ].evidenceValue,
                                    ATTRIBUTE_VALUE_LENGTH, attribute->value);
                validationCopyField(score->factors[ score->factorCount ].suspectValue,
                                    ATTRIBUTE_VALUE_LENGTH, suspectValueText);
                score->factors[ score->factorCount ].weight = weight;
                score->factors[ score->factorCount ].matched = matched;
                score->factorCount++;
            }
        }

        if (result->suspects[ target ].score > MAX_SUSPICION_SCORE)
        {
            result->suspects[ target ].score = MAX_SUSPICION_SCORE;
        }
    }

    investigation_rankSuspects(result->suspects, result->suspectCount);

    return CF_SUCCESS;
}

CfStatus_t investigation_computeProgress(const CasefileDb_t *db, const char *caseId,
                                         int *percent, char *detail, size_t detailSize)
{
    size_t suspectCount;
    size_t witnessCount;
    size_t verifiedCount;
    size_t evidenceCount;
    size_t analysedCount;
    int value = 0;

    if (db == NULL || caseId == NULL || percent == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *percent = 0;

    if (!casefileCaseIdExists(db, caseId))
    {
        return CF_ERR_NOT_FOUND;
    }

    suspectCount = suspectManager_countByCase(db, caseId);
    witnessCount = witnessManager_countByCase(db, caseId);
    verifiedCount = witnessManager_countVerified(db, caseId);
    evidenceCount = evidenceManager_countByCase(db, caseId);
    analysedCount = evidenceManager_countAnalysed(db, caseId);

    /*
     * Three components contribute to progress:
     *   - evidence analysed          (50% of the total)
     *   - suspects registered        (25% of the total)
     *   - statements verified       (25% of the total)
     */
    if (evidenceCount > 0U)
    {
        value += (int)((analysedCount * 50U) / evidenceCount);
    }
    if (suspectCount > 0U)
    {
        value += 25;
    }
    if (witnessCount > 0U)
    {
        value += (int)((verifiedCount * 25U) / witnessCount);
    }

    if (value > 100)
    {
        value = 100;
    }
    if (value < 0)
    {
        value = 0;
    }

    *percent = value;

    if (detail != NULL && detailSize > 0U)
    {
        snprintf(detail, detailSize,
                 "%zu suspect(s), %zu/%zu evidence analysed, %zu/%zu statement(s) verified",
                 suspectCount, analysedCount, evidenceCount, verifiedCount, witnessCount);
    }

    return CF_SUCCESS;
}

const char *investigation_getDisclaimer(void)
{
    return "EDUCATIONAL DECISION SUPPORT: the score summarises how many recorded, "
           "rule-comparable attributes coincide between the recorded evidence and a "
           "suspect's profile. It is NOT a forensic conclusion, NOT a probability "
           "and NOT evidence of guilt. Verify every factor manually before acting.";
}

const char *investigation_describeScore(int score)
{
    if (score >= 75)
    {
        return "very high priority for review";
    }
    if (score >= 50)
    {
        return "high priority for review";
    }
    if (score >= 25)
    {
        return "moderate priority for review";
    }
    if (score > 0)
    {
        return "low priority for review";
    }
    return "no comparable attributes matched";
}
