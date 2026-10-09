// Queries observed through controlled calls to the pinned public connector API.
// Evidence: docs/database-driver-reference-behavior.json
#pragma once
#include <string>
namespace minera::database_history_detail {
inline const std::string camunda_workflow_query=R"MINERASQL(
    SELECT
    pi.PROC_DEF_KEY_ AS "processID",
    ai.EXECUTION_ID_ AS "case:concept:name",
    ai.ACT_NAME_ AS "concept:name",
    ai.START_TIME_ AS "time:timestamp",
    ai.ASSIGNEE_ AS "org:resource"
    FROM
        act_hi_procinst pi
    JOIN
        act_hi_actinst ai ON pi.PROC_INST_ID_ = ai.PROC_INST_ID_
    ORDER BY
        pi.PROC_INST_ID_,
        ai.EXECUTION_ID_,
        ai.START_TIME_;
    )MINERASQL";
inline const std::string sap_accounting_query=R"MINERASQL(
    SELECT * FROM
    (SELECT MANDT || '-' || BUKRS || '-' || GJAHR || '-' || BELNR AS "case:concept:name", 'Create Financial Document - ' || BLART AS "concept:name", CPUDT || ' ' || CPUTM AS "time:timestamp", USNAM AS "org:resource", BLART FROM OWNED.BKPF
    UNION
    SELECT MANDT || '-' || BUKRS || '-' || GJAHR || '-' || AUGBL AS "case:concept:name", 'Clear Customer Document' AS "concept:name", AUGDT || ' 235958' AS "time:timestamp", NULL AS "org:resource", NULL AS BLART FROM OWNED.BSAD
    UNION
    SELECT MANDT || '-' || BUKRS || '-' || GJAHR || '-' || AUGBL AS "case:concept:name", 'Clear Vendor Document' AS "concept:name", AUGDT || ' 235959' AS "time:timestamp", NULL AS "org:resource", NULL AS BLART FROM OWNED.BSAK)
    ORDER BY "case:concept:name", "time:timestamp";
    )MINERASQL";
inline const std::string sap_o2c_query=R"MINERASQL(
SELECT * FROM
(SELECT
    T1.VBELN AS "case:concept:name",
    DECODE(T1.TYPE, 'C', 'Create Sales Document', 'D', 'Create Delivery') AS "concept:name",
    T1.TIMESTAMP AS "time:timestamp",
    T1.USERNAME AS "org:resource"
FROM
    (
        SELECT
            'C' AS TYPE,
            VBAK.VBELN,
            VBAK.ERDAT || ' ' || VBAK.ERZET AS TIMESTAMP,
            VBAK.ERNAM AS USERNAME
        FROM
            OWNED.VBAK
        UNION ALL
        SELECT
            'D' AS TYPE,
            VBFA.VBELV AS VBELN,
            LIKP.ERDAT || ' ' || LIKP.ERZET AS TIMESTAMP,
            LIKP.ERNAM AS USERNAME
        FROM
            OWNED.LIKP
        JOIN OWNED.VBFA ON LIKP.VBELN = VBFA.VBELN
        WHERE
            VBFA.VBTYP_N = 'J'
    ) T1 )
    UNION
    (
    SELECT
    VBFA.VBELN AS "case:concept:name",
    TSTCT.TTEXT AS "concept:name",
    CDHDR.UDATE || ' ' || CDHDR.UTIME AS "time:timestamp",
    CDHDR.USERNAME AS "org:resource"
FROM
    OWNED.VBFA VBFA
    JOIN OWNED.VBAK VBAK ON VBFA.VBELN = VBAK.VBELN
    JOIN OWNED.VBAP VBAP ON VBAK.VBELN = VBAP.VBELN
    JOIN OWNED.CDHDR CDHDR ON VBFA.VBELN = CDHDR.OBJECTID
    JOIN OWNED.TSTCT TSTCT ON CDHDR.TCODE = TSTCT.TCODE
WHERE
    VBFA.VBTYP_N = 'C'
    AND TSTCT.SPRSL = 'E'
    )
    UNION
    (
        SELECT
        C.VBELN AS "case:concept:name",
        'Create Billing' AS "concept:name",
        C.FKDAT AS "time:timestamp",
        C.ERNAM AS "org:resource"
    FROM
        OWNED.VBRK C
    JOIN
        OWNED.VBAK A ON A.VBELN = C.VBELN
    )
    )MINERASQL";
}
