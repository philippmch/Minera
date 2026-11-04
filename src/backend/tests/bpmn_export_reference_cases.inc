// Generated from pm4py 2.7.22 convert_to_bpmn(read_pnml(...)) exported by its BPMN exporter.
// Fields: name, PNML, nodes (element|task name|gatewayDirection), flows (source node>target node).
{"tau_mm.pnml",
R"PNML(<pnml>
  <net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>n</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="p1">
        <name>
          <text>p1</text>
        </name>
      </place>
      <place id="p4">
        <name>
          <text>p4</text>
        </name>
      </place>
      <place id="p3">
        <name>
          <text>p3</text>
        </name>
      </place>
      <place id="p2">
        <name>
          <text>p2</text>
        </name>
      </place>
      <place id="p5">
        <name>
          <text>p5</text>
        </name>
      </place>
      <place id="p6">
        <name>
          <text>p6</text>
        </name>
      </place>
      <place id="p7">
        <name>
          <text>p7</text>
        </name>
      </place>
      <place id="p8">
        <name>
          <text>p8</text>
        </name>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <transition id="a">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="c">
        <name>
          <text>c</text>
        </name>
      </transition>
      <transition id="d">
        <name>
          <text>d</text>
        </name>
      </transition>
      <transition id="b">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="tau">
        <name>
          <text>tau</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="2f97e076-bf5f-477e-9358-61b67434119c"/>
      </transition>
      <transition id="e">
        <name>
          <text>e</text>
        </name>
      </transition>
      <transition id="f">
        <name>
          <text>f</text>
        </name>
      </transition>
      <arc id="140662907206160" source="d" target="p7"/>
      <arc id="140662907206416" source="p6" target="e"/>
      <arc id="140662907206544" source="e" target="p8"/>
      <arc id="140662907206928" source="p8" target="f"/>
      <arc id="140662907204112" source="source" target="a"/>
      <arc id="140662907204368" source="a" target="p2"/>
      <arc id="140662907204624" source="p1" target="b"/>
      <arc id="140662907204240" source="a" target="p1"/>
      <arc id="140662907204752" source="b" target="p3"/>
      <arc id="140662907205008" source="p2" target="c"/>
      <arc id="140662907205136" source="c" target="p4"/>
      <arc id="140662907205392" source="p3" target="tau"/>
      <arc id="140662907205520" source="p4" target="tau"/>
      <arc id="140662907205648" source="tau" target="p5"/>
      <arc id="140662907205776" source="tau" target="p6"/>
      <arc id="140662907206800" source="p7" target="f"/>
      <arc id="140662907206032" source="p5" target="d"/>
      <arc id="140662907207056" source="f" target="sink"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "parallelGateway||Converging", "parallelGateway||Converging", "parallelGateway||Diverging", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified", "task|c|Unspecified", "task|d|Unspecified", "task|e|Unspecified", "task|f|Unspecified"},
{"parallelGateway||Converging>task|d|Unspecified", "parallelGateway||Converging>task|e|Unspecified", "parallelGateway||Converging>task|f|Unspecified", "parallelGateway||Diverging>task|b|Unspecified", "parallelGateway||Diverging>task|c|Unspecified", "startEvent||Unspecified>task|a|Unspecified", "task|a|Unspecified>parallelGateway||Diverging", "task|b|Unspecified>parallelGateway||Converging", "task|c|Unspecified>parallelGateway||Converging", "task|d|Unspecified>parallelGateway||Converging", "task|e|Unspecified>parallelGateway||Converging", "task|f|Unspecified>endEvent||Unspecified"}},
{"unb.pnml",
R"PNML(<pnml>
  <net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>n</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="p">
        <name>
          <text>p</text>
        </name>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <transition id="a">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="b">
        <name>
          <text>b</text>
        </name>
      </transition>
      <arc id="140338732605072" source="source" target="a"/>
      <arc id="140338732605456" source="a" target="p"/>
      <arc id="140338732605584" source="source" target="b"/>
      <arc id="140338732605712" source="b" target="sink"/>
      <arc id="140338739385552" source="a" target="source"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Diverging", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified"},
{"exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|b|Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>parallelGateway||Diverging", "task|b|Unspecified>endEvent||Unspecified"}},
{"alpha1.pnml",
R"PNML(<pnml>
  <net id="alpha_classic_net_1793665341.105499" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>alpha_classic_net_1793665341.105499</text>
    </name>
    <page id="n0">
      <place id="start">
        <name>
          <text>start</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="end">
        <name>
          <text>end</text>
        </name>
      </place>
      <place id="({'c', 'e'}, {'d'})">
        <name>
          <text>({'c', 'e'}, {'d'})</text>
        </name>
      </place>
      <place id="({'a'}, {'c', 'e'})">
        <name>
          <text>({'a'}, {'c', 'e'})</text>
        </name>
      </place>
      <place id="({'a'}, {'b', 'e'})">
        <name>
          <text>({'a'}, {'b', 'e'})</text>
        </name>
      </place>
      <place id="({'b', 'e'}, {'d'})">
        <name>
          <text>({'b', 'e'}, {'d'})</text>
        </name>
      </place>
      <transition id="c">
        <name>
          <text>c</text>
        </name>
      </transition>
      <transition id="e">
        <name>
          <text>e</text>
        </name>
      </transition>
      <transition id="d">
        <name>
          <text>d</text>
        </name>
      </transition>
      <transition id="b">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="a">
        <name>
          <text>a</text>
        </name>
      </transition>
      <arc id="140246598637776" source="e" target="({'b', 'e'}, {'d'})"/>
      <arc id="140246598637392" source="({'a'}, {'c', 'e'})" target="e"/>
      <arc id="140246598635600" source="start" target="a"/>
      <arc id="140246598635856" source="d" target="end"/>
      <arc id="140246598636112" source="a" target="({'a'}, {'b', 'e'})"/>
      <arc id="140246598636368" source="({'a'}, {'b', 'e'})" target="e"/>
      <arc id="140246598636240" source="({'a'}, {'b', 'e'})" target="b"/>
      <arc id="140246598636624" source="c" target="({'c', 'e'}, {'d'})"/>
      <arc id="140246598636752" source="e" target="({'c', 'e'}, {'d'})"/>
      <arc id="140246598636880" source="({'c', 'e'}, {'d'})" target="d"/>
      <arc id="140246598637136" source="a" target="({'a'}, {'c', 'e'})"/>
      <arc id="140246598637264" source="({'a'}, {'c', 'e'})" target="c"/>
      <arc id="140246598637648" source="b" target="({'b', 'e'}, {'d'})"/>
      <arc id="140246598637904" source="({'b', 'e'}, {'d'})" target="d"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="end">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Converging", "parallelGateway||Converging", "parallelGateway||Diverging", "parallelGateway||Diverging", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified", "task|c|Unspecified", "task|d|Unspecified", "task|e|Unspecified"},
{"exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>task|b|Unspecified", "exclusiveGateway||Unspecified>task|c|Unspecified", "parallelGateway||Converging>task|d|Unspecified", "parallelGateway||Converging>task|e|Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "startEvent||Unspecified>task|a|Unspecified", "task|a|Unspecified>parallelGateway||Diverging", "task|b|Unspecified>exclusiveGateway||Unspecified", "task|c|Unspecified>exclusiveGateway||Unspecified", "task|d|Unspecified>endEvent||Unspecified", "task|e|Unspecified>parallelGateway||Diverging"}},
{"tmp_2_16_alpha.pnml",
R"PNML(<pnml>
  <net id="alpha_classic_net_1793665411.9758139" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>alpha_classic_net_1793665411.9758139</text>
    </name>
    <page id="n0">
      <place id="start">
        <name>
          <text>start</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="end">
        <name>
          <text>end</text>
        </name>
      </place>
      <place id="({'b'}, {'d'})">
        <name>
          <text>({'b'}, {'d'})</text>
        </name>
      </place>
      <place id="({'b'}, {'c'})">
        <name>
          <text>({'b'}, {'c'})</text>
        </name>
      </place>
      <transition id="a">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="b">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="c">
        <name>
          <text>c</text>
        </name>
      </transition>
      <transition id="d">
        <name>
          <text>d</text>
        </name>
      </transition>
      <arc id="140028012290448" source="start" target="a"/>
      <arc id="140028012283408" source="start" target="b"/>
      <arc id="140028012295952" source="a" target="end"/>
      <arc id="140028012285072" source="d" target="end"/>
      <arc id="140028012297168" source="c" target="end"/>
      <arc id="140028012288080" source="b" target="({'b'}, {'c'})"/>
      <arc id="140028012287376" source="({'b'}, {'c'})" target="c"/>
      <arc id="140028013420048" source="b" target="({'b'}, {'d'})"/>
      <arc id="140028013424080" source="({'b'}, {'d'})" target="d"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="end">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Diverging", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified", "task|c|Unspecified", "task|d|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|b|Unspecified", "parallelGateway||Diverging>task|c|Unspecified", "parallelGateway||Diverging>task|d|Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|b|Unspecified>parallelGateway||Diverging", "task|c|Unspecified>exclusiveGateway||Unspecified", "task|d|Unspecified>exclusiveGateway||Unspecified"}},
{"tmp_3_13_heu.pnml",
R"PNML(<pnml>
  <net id="" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text></text>
    </name>
    <page id="n0">
      <place id="source0">
        <name>
          <text>source0</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="sink0">
        <name>
          <text>sink0</text>
        </name>
      </place>
      <place id="intplace_a">
        <name>
          <text>intplace_a</text>
        </name>
      </place>
      <transition id="d">
        <name>
          <text>d</text>
        </name>
      </transition>
      <transition id="a">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="b">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="c">
        <name>
          <text>c</text>
        </name>
      </transition>
      <arc id="140163409825168" source="source0" target="d"/>
      <arc id="140163407901776" source="source0" target="c"/>
      <arc id="140163409823312" source="source0" target="a"/>
      <arc id="140163407900752" source="d" target="sink0"/>
      <arc id="140163409836688" source="source0" target="b"/>
      <arc id="140163409482768" source="a" target="intplace_a"/>
      <arc id="140163408225232" source="b" target="sink0"/>
      <arc id="140163408221328" source="c" target="sink0"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink0">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified", "task|c|Unspecified", "task|d|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|b|Unspecified", "exclusiveGateway||Unspecified>task|c|Unspecified", "exclusiveGateway||Unspecified>task|d|Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|b|Unspecified>exclusiveGateway||Unspecified", "task|c|Unspecified>exclusiveGateway||Unspecified", "task|d|Unspecified>exclusiveGateway||Unspecified"}},
{"tmp_3_16_ind.pnml",
R"PNML(<pnml>
  <net id="imdf_net_1793665414.5239916" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>imdf_net_1793665414.5239916</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="p_3">
        <name>
          <text>p_3</text>
        </name>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <place id="p_5">
        <name>
          <text>p_5</text>
        </name>
      </place>
      <place id="p_4">
        <name>
          <text>p_4</text>
        </name>
      </place>
      <transition id="a910b982-560c-4afd-8895-a14686ea0f52">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="86c12d37-c5e5-466f-97bb-ada840026250">
        <name>
          <text>d</text>
        </name>
      </transition>
      <transition id="1f20c891-3337-49fb-b8a7-2fcb8f8befe1">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="5e0446ae-9754-4130-9970-49b6a0207948">
        <name>
          <text>e</text>
        </name>
      </transition>
      <transition id="26812ca8-59ff-422f-8f70-ee910ab15113">
        <name>
          <text>c</text>
        </name>
      </transition>
      <arc id="140705491387600" source="source" target="a910b982-560c-4afd-8895-a14686ea0f52"/>
      <arc id="140705491395344" source="p_3" target="86c12d37-c5e5-466f-97bb-ada840026250"/>
      <arc id="140705491401424" source="p_3" target="1f20c891-3337-49fb-b8a7-2fcb8f8befe1"/>
      <arc id="140705491399632" source="1f20c891-3337-49fb-b8a7-2fcb8f8befe1" target="p_4"/>
      <arc id="140705491401168" source="a910b982-560c-4afd-8895-a14686ea0f52" target="p_3"/>
      <arc id="140705491399696" source="86c12d37-c5e5-466f-97bb-ada840026250" target="p_4"/>
      <arc id="140705491396304" source="p_4" target="26812ca8-59ff-422f-8f70-ee910ab15113"/>
      <arc id="140705491397776" source="26812ca8-59ff-422f-8f70-ee910ab15113" target="p_5"/>
      <arc id="140705491387728" source="p_5" target="5e0446ae-9754-4130-9970-49b6a0207948"/>
      <arc id="140705491396240" source="5e0446ae-9754-4130-9970-49b6a0207948" target="sink"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified", "task|c|Unspecified", "task|d|Unspecified", "task|e|Unspecified"},
{"exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|c|Unspecified", "exclusiveGateway||Unspecified>task|d|Unspecified", "startEvent||Unspecified>task|b|Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|b|Unspecified>exclusiveGateway||Unspecified", "task|c|Unspecified>task|e|Unspecified", "task|d|Unspecified>exclusiveGateway||Unspecified", "task|e|Unspecified>endEvent||Unspecified"}},
{"tmp_3_10_ind.pnml",
R"PNML(<pnml>
  <net id="imdf_net_1793669052.8935328" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>imdf_net_1793669052.8935328</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="p_4">
        <name>
          <text>p_4</text>
        </name>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <place id="p_5">
        <name>
          <text>p_5</text>
        </name>
      </place>
      <place id="p_6">
        <name>
          <text>p_6</text>
        </name>
      </place>
      <place id="p_7">
        <name>
          <text>p_7</text>
        </name>
      </place>
      <transition id="dc0c806e-b495-440d-8b99-87dccb45af1e">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="tauJoin_2">
        <name>
          <text>tauJoin_2</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="05807164-e454-424f-a100-5a3391706f91"/>
      </transition>
      <transition id="3cb10927-b42e-4cd1-adb2-502f59d41fa7">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="eaf75062-0bc5-4700-93f6-a9202455e747">
        <name>
          <text>c</text>
        </name>
      </transition>
      <transition id="816cf752-c926-4483-84ee-8a868b477308">
        <name>
          <text>d</text>
        </name>
      </transition>
      <arc id="140163410631952" source="816cf752-c926-4483-84ee-8a868b477308" target="p_7"/>
      <arc id="140163410674384" source="source" target="dc0c806e-b495-440d-8b99-87dccb45af1e"/>
      <arc id="140163410686224" source="tauJoin_2" target="sink"/>
      <arc id="140163410632400" source="dc0c806e-b495-440d-8b99-87dccb45af1e" target="p_6"/>
      <arc id="140163410678416" source="p_5" target="tauJoin_2"/>
      <arc id="140163411899856" source="p_4" target="3cb10927-b42e-4cd1-adb2-502f59d41fa7"/>
      <arc id="140163411898448" source="3cb10927-b42e-4cd1-adb2-502f59d41fa7" target="p_5"/>
      <arc id="140163410629072" source="dc0c806e-b495-440d-8b99-87dccb45af1e" target="p_4"/>
      <arc id="140163425786960" source="p_7" target="tauJoin_2"/>
      <arc id="140163410449680" source="p_6" target="eaf75062-0bc5-4700-93f6-a9202455e747"/>
      <arc id="140163410448464" source="eaf75062-0bc5-4700-93f6-a9202455e747" target="p_7"/>
      <arc id="140163410903504" source="p_6" target="816cf752-c926-4483-84ee-8a868b477308"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Converging", "parallelGateway||Diverging", "startEvent||Unspecified", "task|a|Unspecified", "task|b|Unspecified", "task|c|Unspecified", "task|d|Unspecified"},
{"exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>task|c|Unspecified", "exclusiveGateway||Unspecified>task|d|Unspecified", "parallelGateway||Converging>endEvent||Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "parallelGateway||Diverging>task|a|Unspecified", "startEvent||Unspecified>task|b|Unspecified", "task|a|Unspecified>parallelGateway||Converging", "task|b|Unspecified>parallelGateway||Diverging", "task|c|Unspecified>exclusiveGateway||Unspecified", "task|d|Unspecified>exclusiveGateway||Unspecified"}},
// Duplicate-label nets (2026-11-03 review): pm4py.write_bpmn(convert_to_bpmn(...), auto_layout=False) keeps one task per transition.
{"dup_label_n11.pnml",
R"PNML(<?xml version='1.0' encoding='UTF-8'?>
<pnml>
  <net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>n</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="p2">
        <name>
          <text>p2</text>
        </name>
      </place>
      <place id="p1">
        <name>
          <text>p1</text>
        </name>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <place id="p0">
        <name>
          <text>p0</text>
        </name>
      </place>
      <transition id="t0">
        <name>
          <text>b</text>
        </name>
      </transition>
      <transition id="t1">
        <name>
          <text>t1</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="4be46cf4-3359-43fc-8819-6deaa3ff08cb"/>
      </transition>
      <transition id="t3">
        <name>
          <text>t3</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="225ffb3c-cb37-4409-9b24-da2a22b7e867"/>
      </transition>
      <transition id="t2">
        <name>
          <text>b</text>
        </name>
      </transition>
      <arc id="140336698704656" source="source" target="t1"/>
      <arc id="140336698704976" source="p1" target="t1"/>
      <arc id="140336698704848" source="source" target="t2"/>
      <arc id="140336698704784" source="t2" target="p2"/>
      <arc id="140336698705104" source="t1" target="p2"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Converging", "startEvent||Unspecified", "task|b|Unspecified", "task|b|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>task|b|Unspecified", "exclusiveGateway||Unspecified>task|b|Unspecified", "parallelGateway||Converging>exclusiveGateway||Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|b|Unspecified>exclusiveGateway||Unspecified", "task|b|Unspecified>exclusiveGateway||Unspecified"}},
{"dup_label_n31.pnml",
R"PNML(<?xml version='1.0' encoding='UTF-8'?>
<pnml>
  <net id="imdf_net_1793724533.620046" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>imdf_net_1793724533.620046</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="p_4">
        <name>
          <text>p_4</text>
        </name>
      </place>
      <place id="p_3">
        <name>
          <text>p_3</text>
        </name>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <place id="p_6">
        <name>
          <text>p_6</text>
        </name>
      </place>
      <transition id="tau_1">
        <name>
          <text>tau_1</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="810cd5ae-39d5-4804-81f3-b74e697b8285"/>
      </transition>
      <transition id="tau_2">
        <name>
          <text>tau_2</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="28bd2032-96f4-44b3-81d5-28bb86f25ceb"/>
      </transition>
      <transition id="996f95c2-f595-443b-ad34-3a29ce55d3f3">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="f9188fe6-86ce-484c-863a-7aa4cc1d9b51">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="3aa237cb-b416-4296-a8f6-f02a68f58c57">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="bb3a92ea-ceb9-4918-97b1-17e11efe4213">
        <name>
          <text>a</text>
        </name>
      </transition>
      <arc id="140336698700048" source="3aa237cb-b416-4296-a8f6-f02a68f58c57" target="p_4"/>
      <arc id="140336698688784" source="source" target="tau_1"/>
      <arc id="140336698688528" source="tau_1" target="p_3"/>
      <arc id="140336698692112" source="p_3" target="f9188fe6-86ce-484c-863a-7aa4cc1d9b51"/>
      <arc id="140336698701008" source="tau_2" target="sink"/>
      <arc id="140336698687696" source="p_4" target="tau_2"/>
      <arc id="140336698699728" source="f9188fe6-86ce-484c-863a-7aa4cc1d9b51" target="p_4"/>
      <arc id="140336698646928" source="p_3" target="996f95c2-f595-443b-ad34-3a29ce55d3f3"/>
      <arc id="140336698700944" source="996f95c2-f595-443b-ad34-3a29ce55d3f3" target="p_6"/>
      <arc id="140336698699920" source="p_6" target="3aa237cb-b416-4296-a8f6-f02a68f58c57"/>
      <arc id="140336698700176" source="bb3a92ea-ceb9-4918-97b1-17e11efe4213" target="p_4"/>
      <arc id="140336698700240" source="p_3" target="bb3a92ea-ceb9-4918-97b1-17e11efe4213"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|a|Unspecified", "task|a|Unspecified", "task|a|Unspecified", "task|a|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|a|Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>task|a|Unspecified"}},
{"dup_label_n32.pnml",
R"PNML(<?xml version='1.0' encoding='UTF-8'?>
<pnml>
  <net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>n</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <place id="p0">
        <name>
          <text>p0</text>
        </name>
      </place>
      <transition id="t0">
        <name>
          <text>a</text>
        </name>
      </transition>
      <transition id="t2">
        <name>
          <text>t2</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="4a895080-0844-4d16-8bc6-50e467a02b1a"/>
      </transition>
      <transition id="t1">
        <name>
          <text>a</text>
        </name>
      </transition>
      <arc id="140336698638800" source="source" target="t0"/>
      <arc id="140336698640912" source="t0" target="p0"/>
      <arc id="140336698641936" source="source" target="t2"/>
      <arc id="140336698641744" source="t0" target="sink"/>
      <arc id="140336698639056" source="p0" target="t1"/>
      <arc id="140336698641040" source="p0" target="t2"/>
      <arc id="140336698640272" source="t2" target="p0"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Converging", "parallelGateway||Diverging", "startEvent||Unspecified", "task|a|Unspecified", "task|a|Unspecified"},
{"exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>task|a|Unspecified", "exclusiveGateway||Unspecified>task|a|Unspecified", "parallelGateway||Converging>exclusiveGateway||Unspecified", "parallelGateway||Diverging>endEvent||Unspecified", "parallelGateway||Diverging>exclusiveGateway||Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>exclusiveGateway||Unspecified", "task|a|Unspecified>parallelGateway||Diverging"}},
{"dup_label_n38.pnml",
R"PNML(<?xml version='1.0' encoding='UTF-8'?>
<pnml>
  <net id="imdf_net_1793724533.6220288" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name>
      <text>imdf_net_1793724533.6220288</text>
    </name>
    <page id="n0">
      <place id="source">
        <name>
          <text>source</text>
        </name>
        <initialMarking>
          <text>1</text>
        </initialMarking>
      </place>
      <place id="sink">
        <name>
          <text>sink</text>
        </name>
      </place>
      <place id="p_5">
        <name>
          <text>p_5</text>
        </name>
      </place>
      <place id="p_6">
        <name>
          <text>p_6</text>
        </name>
      </place>
      <place id="p_8">
        <name>
          <text>p_8</text>
        </name>
      </place>
      <place id="p_9">
        <name>
          <text>p_9</text>
        </name>
      </place>
      <transition id="skip_7">
        <name>
          <text>skip_7</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="186596f5-d86d-462e-b3f8-ad03279d4de3"/>
      </transition>
      <transition id="skip_8">
        <name>
          <text>skip_8</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="0e3bae8e-c8e0-4775-b30a-9fdf8a314f6d"/>
      </transition>
      <transition id="skip_9">
        <name>
          <text>skip_9</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="1479d480-7773-41f4-bafd-5f31ecb029e2"/>
      </transition>
      <transition id="tau_1">
        <name>
          <text>tau_1</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="53117527-0bc0-489b-bf31-fb851adde62c"/>
      </transition>
      <transition id="44ab5530-3a87-41de-ad4c-33708c5330fb">
        <name>
          <text>c</text>
        </name>
      </transition>
      <transition id="cc8adb08-7538-43f8-9969-c017ff8d2016">
        <name>
          <text>é</text>
        </name>
      </transition>
      <transition id="f3bc9d85-d2cd-44d9-92d2-c94fababfa4b">
        <name>
          <text>d</text>
        </name>
      </transition>
      <transition id="init_loop_5">
        <name>
          <text>init_loop_5</text>
        </name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="333e617d-c383-4082-8dba-3ddba6a8aa7d"/>
      </transition>
      <transition id="cfe48e7b-3055-423f-97ef-dacfe96dc80b">
        <name>
          <text>é</text>
        </name>
      </transition>
      <arc id="140336698715984" source="skip_8" target="p_5"/>
      <arc id="140336698769360" source="p_8" target="cfe48e7b-3055-423f-97ef-dacfe96dc80b"/>
      <arc id="140336698768016" source="cfe48e7b-3055-423f-97ef-dacfe96dc80b" target="p_9"/>
      <arc id="140336698766992" source="p_9" target="skip_8"/>
      <arc id="140336698704528" source="skip_9" target="sink"/>
      <arc id="140336698793360" source="p_5" target="cc8adb08-7538-43f8-9969-c017ff8d2016"/>
      <arc id="140336698793488" source="cc8adb08-7538-43f8-9969-c017ff8d2016" target="p_6"/>
      <arc id="140336698758416" source="p_6" target="skip_9"/>
      <arc id="140336698705168" source="tau_1" target="p_5"/>
      <arc id="140336698767568" source="p_9" target="skip_7"/>
      <arc id="140336698794704" source="p_6" target="init_loop_5"/>
      <arc id="140336698793808" source="p_5" target="f3bc9d85-d2cd-44d9-92d2-c94fababfa4b"/>
      <arc id="140336698794832" source="init_loop_5" target="p_8"/>
      <arc id="140336698793936" source="f3bc9d85-d2cd-44d9-92d2-c94fababfa4b" target="p_6"/>
      <arc id="140336698790608" source="source" target="tau_1"/>
      <arc id="140336698792912" source="p_5" target="44ab5530-3a87-41de-ad4c-33708c5330fb"/>
      <arc id="140336698714576" source="skip_7" target="p_8"/>
      <arc id="140336698793040" source="44ab5530-3a87-41de-ad4c-33708c5330fb" target="p_6"/>
    </page>
    <finalmarkings>
      <marking>
        <place idref="sink">
          <text>1</text>
        </place>
      </marking>
    </finalmarkings>
  </net>
</pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|c|Unspecified", "task|d|Unspecified", "task|é|Unspecified", "task|é|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified>exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified>exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified>task|c|Unspecified", "exclusiveGateway||Unspecified>task|d|Unspecified", "exclusiveGateway||Unspecified>task|é|Unspecified", "exclusiveGateway||Unspecified>task|é|Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|c|Unspecified>exclusiveGateway||Unspecified", "task|d|Unspecified>exclusiveGateway||Unspecified", "task|é|Unspecified>exclusiveGateway||Unspecified", "task|é|Unspecified>exclusiveGateway||Unspecified"}},
// General nets read with their markings (pm4py 2.7.22 convert_to_bpmn(net, im, fm)):
// several marked places, empty markings, a place in both markings, weights.
{"general_two_starts_two_ends.pnml",
R"PNML(<pnml><net id="net1" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="n0">
<place id="p0"><name><text>p0</text></name><initialMarking><text>1</text></initialMarking></place>
<place id="p1"><name><text>p1</text></name><initialMarking><text>1</text></initialMarking></place>
<place id="p2"><name><text>p2</text></name></place>
<place id="p3"><name><text>p3</text></name></place>
<transition id="t0"><name><text>A</text></name></transition>
<transition id="t1"><name><text>B</text></name></transition>
<arc id="a0" source="p0" target="t0"></arc>
<arc id="a1" source="t0" target="p2"></arc>
<arc id="a2" source="p1" target="t1"></arc>
<arc id="a3" source="t1" target="p3"></arc>
</page><finalmarkings><marking><place idref="p2"><text>1</text></place><place idref="p3"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML",
{"endEvent||Unspecified", "startEvent||Unspecified", "task|A|Unspecified", "task|B|Unspecified"},
{"startEvent||Unspecified>task|A|Unspecified", "startEvent||Unspecified>task|B|Unspecified", "task|A|Unspecified>endEvent||Unspecified", "task|B|Unspecified>endEvent||Unspecified"}},
{"general_empty_markings.pnml",
R"PNML(<pnml><net id="net1" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="n0">
<place id="p0"><name><text>p0</text></name></place>
<place id="p1"><name><text>p1</text></name></place>
<transition id="t0"><name><text>A</text></name></transition>
<arc id="a0" source="p0" target="t0"></arc>
<arc id="a1" source="t0" target="p1"></arc>
</page><finalmarkings><marking></marking></finalmarkings></net></pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|A|Unspecified"},
{"exclusiveGateway||Unspecified>task|A|Unspecified", "task|A|Unspecified>exclusiveGateway||Unspecified"}},
{"general_isolated_marked_place.pnml",
R"PNML(<pnml><net id="net1" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="n0">
<place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place>
<place id="sink"><name><text>sink</text></name></place>
<place id="q"><name><text>q</text></name><initialMarking><text>1</text></initialMarking></place>
<transition id="t0"><name><text>A</text></name></transition>
<arc id="a0" source="source" target="t0"></arc>
<arc id="a1" source="t0" target="sink"></arc>
</page><finalmarkings><marking><place idref="sink"><text>1</text></place><place idref="q"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML",
{"endEvent||Unspecified", "startEvent||Unspecified", "task|A|Unspecified"},
{"startEvent||Unspecified>endEvent||Unspecified", "startEvent||Unspecified>task|A|Unspecified", "task|A|Unspecified>endEvent||Unspecified"}},
{"general_pass_through_weighted.pnml",
R"PNML(<pnml><net id="net1" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="n0">
<place id="p0"><name><text>p0</text></name><initialMarking><text>2</text></initialMarking></place>
<place id="p1"><name><text>p1</text></name></place>
<place id="p2"><name><text>p2</text></name></place>
<transition id="t0"><name><text>A</text></name></transition>
<transition id="t1"><name><text>t1</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="xt1"/></transition>
<transition id="t2"><name><text>B</text></name></transition>
<arc id="a0" source="p0" target="t0"><inscription><text>2</text></inscription></arc>
<arc id="a1" source="t0" target="p1"></arc>
<arc id="a2" source="p1" target="t1"></arc>
<arc id="a3" source="t1" target="p0"></arc>
<arc id="a4" source="p1" target="t2"></arc>
<arc id="a5" source="t2" target="p2"><inscription><text>3</text></inscription></arc>
</page><finalmarkings><marking><place idref="p0"><text>1</text></place><place idref="p2"><text>3</text></place></marking></finalmarkings></net></pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|A|Unspecified", "task|B|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified>task|A|Unspecified", "exclusiveGateway||Unspecified>task|B|Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|A|Unspecified>exclusiveGateway||Unspecified", "task|B|Unspecified>endEvent||Unspecified"}},
{"general_inner_final_place.pnml",
R"PNML(<pnml><net id="net1" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="n0">
<place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place>
<place id="p1"><name><text>p1</text></name></place>
<place id="sink"><name><text>sink</text></name></place>
<transition id="t0"><name><text>A</text></name></transition>
<transition id="t1"><name><text>B</text></name></transition>
<arc id="a0" source="source" target="t0"></arc>
<arc id="a1" source="t0" target="p1"></arc>
<arc id="a2" source="p1" target="t1"></arc>
<arc id="a3" source="t1" target="sink"></arc>
</page><finalmarkings><marking><place idref="p1"><text>1</text></place><place idref="sink"><text>2</text></place></marking></finalmarkings></net></pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "startEvent||Unspecified", "task|A|Unspecified", "task|B|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>task|B|Unspecified", "startEvent||Unspecified>task|A|Unspecified", "task|A|Unspecified>exclusiveGateway||Unspecified", "task|B|Unspecified>endEvent||Unspecified"}},
{"general_random_weighted.pnml",
R"PNML(<pnml><net id="net1" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="n0">
<place id="p0"><name><text>p0</text></name></place>
<place id="p1"><name><text>p1</text></name><initialMarking><text>2</text></initialMarking></place>
<place id="p2"><name><text>p2</text></name><initialMarking><text>3</text></initialMarking></place>
<place id="p3"><name><text>p3</text></name></place>
<place id="p4"><name><text>p4</text></name></place>
<transition id="t0"><name><text>C</text></name></transition>
<transition id="t1"><name><text>A</text></name></transition>
<arc id="a0" source="p4" target="t0"></arc>
<arc id="a1" source="t0" target="p3"></arc>
<arc id="a2" source="t1" target="p0"><inscription><text>2</text></inscription></arc>
<arc id="a3" source="p4" target="t1"></arc>
<arc id="a4" source="p3" target="t1"></arc>
<arc id="a5" source="p1" target="t1"></arc>
<arc id="a6" source="p1" target="t0"></arc>
</page><finalmarkings><marking><place idref="p0"><text>3</text></place><place idref="p1"><text>2</text></place></marking></finalmarkings></net></pnml>)PNML",
{"endEvent||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "exclusiveGateway||Unspecified", "parallelGateway||Converging", "parallelGateway||Converging", "startEvent||Unspecified", "task|A|Unspecified", "task|C|Unspecified"},
{"exclusiveGateway||Unspecified>endEvent||Unspecified", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "exclusiveGateway||Unspecified>parallelGateway||Converging", "parallelGateway||Converging>task|A|Unspecified", "parallelGateway||Converging>task|C|Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "startEvent||Unspecified>exclusiveGateway||Unspecified", "task|A|Unspecified>endEvent||Unspecified", "task|C|Unspecified>parallelGateway||Converging"}},
