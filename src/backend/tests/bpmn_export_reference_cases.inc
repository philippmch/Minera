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
