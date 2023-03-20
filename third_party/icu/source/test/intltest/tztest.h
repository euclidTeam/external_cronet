// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

/********************************************************************
 * Copyright (c) 1997-2014, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************/
 
#ifndef __TimeZoneTest__
#define __TimeZoneTest__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/simpletz.h" 
#include "caltztst.h"

/** 
 * Various tests for TimeZone
 **/
class TimeZoneTest: public CalendarTimeZoneTest {
    // IntlTest override
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par ) override;
public: // package
    static const int32_t millisPerHour;
 
public:
    /**
     * Test the offset of the PRT timezone.
     */
    virtual void TestPRTOffset(void);
    /**
     * Regress a specific bug with a sequence of API calls.
     */
    virtual void TestVariousAPI518(void);
    /**
     * Test the call which retrieves the available IDs.
     */
    virtual void TestGetAvailableIDs913(void);

    virtual void TestGetAvailableIDsNew(void);

    /**
     * Generic API testing for API coverage.
     */
    virtual void TestGenericAPI(void);
    /**
     * Test the setStartRule/setEndRule API calls.
     */
    virtual void TestRuleAPI(void);
 
    void findTransition(const TimeZone& tz,
                        UDate min, UDate max);

   /**
     * subtest used by TestRuleAPI
     **/
    void testUsingBinarySearch(const TimeZone& tz,
                               UDate min, UDate max,
                               UDate expectedBoundary);


    /**
     *  Test short zone IDs for compliance
     */ 
    virtual void TestShortZoneIDs(void);


    /**
     *  Test parsing custom zones
     */ 
    virtual void TestCustomParse(void);
    
    /**
     *  Test new getDisplayName() API
     */ 
    virtual void TestDisplayName(void);

    void TestDSTSavings(void);
    void TestAlternateRules(void);

    void TestCountries(void);

    void TestHistorical(void);

    void TestEquivalentIDs(void);

    void TestAliasedNames(void);
    
    void TestFractionalDST(void);

    void TestFebruary(void);

    void TestCanonicalIDAPI();
    void TestCanonicalID(void);

    virtual void TestDisplayNamesMeta();

    void TestGetRegion(void);
    void TestGetUnknown();
    void TestGetGMT();

    void TestGetWindowsID(void);
    void TestGetIDForWindowsID(void);
    void TestCasablancaNameAndOffset22041(void);
    void TestRawOffsetAndOffsetConsistency22041(void);

    static const UDate INTERVAL;

private:
    // internal functions
    static UnicodeString& formatOffset(int32_t offset, UnicodeString& rv);
    static UnicodeString& formatTZID(int32_t offset, UnicodeString& rv);

    // Some test case data is current date/tzdata version sensitive and producing errors
    // when year/rule are changed.
    static const int32_t REFERENCE_YEAR;
    static const char *REFERENCE_DATA_VERSION;

    void checkContainsAll(StringEnumeration *s1, const char *name1,
        StringEnumeration *s2, const char *name2);
};

#endif /* #if !UCONFIG_NO_FORMATTING */
 
#endif // __TimeZoneTest__
