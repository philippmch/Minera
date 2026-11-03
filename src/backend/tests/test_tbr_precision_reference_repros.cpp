// Minimal reference repros for forward token replay hidden routing and token
// ETConformance precision. Every expected value was captured from the pinned
// 2.7.22 reference (token replay apply / precision_token_based_replay) on the
// embedded documents, read with the public PNML/PTML importers, and is stable
// across PYTHONHASHSEED values and allocation paddings in fresh processes.
#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/petri_net.hpp"
#include "minera/pnml.hpp"
#include "minera/process_tree.hpp"
#include "minera/ptml.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kM1 = R"PNML(<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>n</text></name><page id="n0"><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="p2"><name><text>p2</text></name></place><place id="p4"><name><text>p4</text></name></place><place id="p3"><name><text>p3</text></name></place><place id="p1"><name><text>p1</text></name></place><place id="sink"><name><text>sink</text></name></place><transition id="tS"><name><text>S</text></name></transition><transition id="a_w"><name><text>a_w</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="5e570dc6-4737-4e2f-8608-2c206172a8db"/></transition><transition id="tB"><name><text>B</text></name></transition><transition id="b_t"><name><text>b_t</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="662acbcd-54f7-443c-b040-6b83ee6bfd94"/></transition><transition id="tA"><name><text>A</text></name></transition><arc id="139933686694416" source="p4" target="tB"/><arc id="139933688659472" source="tB" target="sink"/><arc id="139933688659728" source="p3" target="tB"/><arc id="139933688658064" source="source" target="tS"/><arc id="139933688658448" source="tS" target="p2"/><arc id="139933688656144" source="p2" target="a_w"/><arc id="139933688658256" source="a_w" target="p4"/><arc id="139933688658320" source="tS" target="p1"/><arc id="139933688658576" source="p1" target="b_t"/><arc id="139933688658640" source="b_t" target="p3"/><arc id="139933688658960" source="p3" target="tA"/><arc id="139933688659856" source="p2" target="tA"/><arc id="139933688659024" source="tA" target="sink"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kM1Renamed = R"PNML(<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>n</text></name><page id="n0"><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="p2"><name><text>p2</text></name></place><place id="p3"><name><text>p3</text></name></place><place id="p4"><name><text>p4</text></name></place><place id="p1"><name><text>p1</text></name></place><place id="sink"><name><text>sink</text></name></place><transition id="tS"><name><text>S</text></name></transition><transition id="tB"><name><text>B</text></name></transition><transition id="tA"><name><text>A</text></name></transition><transition id="z_w"><name><text>z_w</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="d9ed0c13-92c1-4a1b-8a68-54aad77579fa"/></transition><transition id="b_t"><name><text>b_t</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="caccca8f-c38e-436c-b849-40a1ee4f7473"/></transition><arc id="140347982066384" source="source" target="tS"/><arc id="140347982066768" source="tS" target="p2"/><arc id="140347988782800" source="p2" target="z_w"/><arc id="140347982066576" source="z_w" target="p4"/><arc id="140347982066640" source="tS" target="p1"/><arc id="140347982066896" source="p1" target="b_t"/><arc id="140347982066960" source="b_t" target="p3"/><arc id="140347982067280" source="p3" target="tA"/><arc id="140347982068176" source="p2" target="tA"/><arc id="140347982067344" source="tA" target="sink"/><arc id="140347982068752" source="p4" target="tB"/><arc id="140347982068048" source="p3" target="tB"/><arc id="140347982067792" source="tB" target="sink"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kM1Root = R"PNML(<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>n</text></name><page id="n0"><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="p4"><name><text>p4</text></name></place><place id="p1"><name><text>p1</text></name></place><place id="p2"><name><text>p2</text></name></place><place id="p3"><name><text>p3</text></name></place><place id="sink"><name><text>sink</text></name></place><transition id="s0"><name><text>s0</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="039438f6-284a-400b-accf-6490efe6ad59"/></transition><transition id="b_t"><name><text>b_t</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="dd6fbf98-7e11-4a2c-93d4-ad30d249964b"/></transition><transition id="tA"><name><text>A</text></name></transition><transition id="tB"><name><text>B</text></name></transition><transition id="a_w"><name><text>a_w</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="63370a22-d75e-428f-842c-8ca648810b5b"/></transition><arc id="140702049484368" source="p4" target="tB"/><arc id="140702049483408" source="tB" target="sink"/><arc id="140702049483664" source="p3" target="tB"/><arc id="140702049480592" source="source" target="s0"/><arc id="140702049482256" source="s0" target="p1"/><arc id="140702056283728" source="p2" target="a_w"/><arc id="140702049482384" source="s0" target="p2"/><arc id="140702049482192" source="a_w" target="p4"/><arc id="140702049482512" source="p1" target="b_t"/><arc id="140702049482576" source="b_t" target="p3"/><arc id="140702049482896" source="p3" target="tA"/><arc id="140702049483792" source="p2" target="tA"/><arc id="140702049482960" source="tA" target="sink"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kM3 = R"PNML(<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>n</text></name><page id="n0"><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="m2"><name><text>m2</text></name></place><place id="m3"><name><text>m3</text></name></place><place id="x"><name><text>x</text></name></place><place id="m1"><name><text>m1</text></name></place><place id="y"><name><text>y</text></name></place><place id="z"><name><text>z</text></name></place><place id="w"><name><text>w</text></name></place><place id="pt"><name><text>pt</text></name></place><place id="sink"><name><text>sink</text></name></place><transition id="tS"><name><text>S</text></name></transition><transition id="c"><name><text>c</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="ca0189c5-58c4-45fc-9e20-5fcfb1f6b229"/></transition><transition id="b"><name><text>b</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="c2ac0a9a-d0fc-45ca-a0c7-8df48c0a6f2b"/></transition><transition id="d"><name><text>d</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="f9ed58e7-b715-4000-a939-ef96f818f13b"/></transition><transition id="a"><name><text>a</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="feb6653f-d6ab-45c7-a523-78e8498a34b4"/></transition><transition id="e"><name><text>e</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="dff70d31-8567-4b39-a7d8-58362d1fc848"/></transition><transition id="f"><name><text>f</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="89d52b45-809c-412d-af19-d63185ab8289"/></transition><transition id="tT"><name><text>T</text></name></transition><arc id="140129588952720" source="w" target="f"/><arc id="140129588951824" source="m2" target="d"/><arc id="140129588952848" source="m3" target="e"/><arc id="140129588951312" source="c" target="y"/><arc id="140129586987280" source="f" target="pt"/><arc id="140129588950416" source="source" target="tS"/><arc id="140129588950160" source="m1" target="a"/><arc id="140129588950672" source="a" target="x"/><arc id="140129588950544" source="x" target="b"/><arc id="140129588951568" source="y" target="b"/><arc id="140129588951696" source="b" target="pt"/><arc id="140129588951952" source="m2" target="c"/><arc id="140129588952208" source="e" target="w"/><arc id="140129586987728" source="pt" target="tT"/><arc id="140129588950992" source="tS" target="m3"/><arc id="140129588950864" source="tS" target="m2"/><arc id="140129588950736" source="tS" target="m1"/><arc id="140129588952400" source="d" target="pt"/><arc id="140129588952528" source="z" target="d"/><arc id="140129588953040" source="tT" target="sink"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kM4 = R"PNML(<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>n</text></name><page id="n0"><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="m1"><name><text>m1</text></name></place><place id="m2"><name><text>m2</text></name></place><place id="x"><name><text>x</text></name></place><place id="m3"><name><text>m3</text></name></place><place id="y"><name><text>y</text></name></place><place id="z"><name><text>z</text></name></place><place id="w"><name><text>w</text></name></place><place id="pt"><name><text>pt</text></name></place><place id="sink"><name><text>sink</text></name></place><transition id="tS"><name><text>S</text></name></transition><transition id="d"><name><text>d</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="d5ccbc64-f584-4294-a759-e6bd9ed1c18a"/></transition><transition id="b"><name><text>b</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="714cd7b8-32a1-4a90-850c-08b0ccd5245d"/></transition><transition id="c"><name><text>c</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="8a582fae-4a88-43e6-b4cd-83507171429d"/></transition><transition id="a"><name><text>a</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="fad8d6c2-8e68-42d0-9a53-83601351f613"/></transition><transition id="e"><name><text>e</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="c777271f-2d51-42ce-ab56-6ccb84b22be1"/></transition><transition id="f"><name><text>f</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="c56843d1-953e-49d1-9f82-0743e200ce28"/></transition><transition id="tT"><name><text>T</text></name></transition><transition id="tU"><name><text>U</text></name></transition><arc id="139724652070672" source="tU" target="sink"/><arc id="139724654034448" source="c" target="y"/><arc id="139724652070416" source="f" target="pt"/><arc id="139724652069520" source="d" target="pt"/><arc id="139724652069648" source="z" target="d"/><arc id="139724654033552" source="source" target="tS"/><arc id="139724654033296" source="m1" target="a"/><arc id="139724654033808" source="a" target="x"/><arc id="139724654033680" source="x" target="b"/><arc id="139724654034704" source="y" target="b"/><arc id="139724654034832" source="b" target="pt"/><arc id="139724652069840" source="w" target="f"/><arc id="139724654034000" source="tS" target="m2"/><arc id="139724654034128" source="tS" target="m3"/><arc id="139724654033872" source="tS" target="m1"/><arc id="139724652069072" source="m2" target="c"/><arc id="139724652068944" source="m2" target="d"/><arc id="139724652069968" source="m3" target="e"/><arc id="139724652070864" source="pt" target="tT"/><arc id="139724652069328" source="e" target="w"/><arc id="139724652070608" source="w" target="tU"/><arc id="139724652070160" source="tT" target="sink"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kM5 = R"PNML(<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>n</text></name><page id="n0"><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="m1"><name><text>m1</text></name></place><place id="m2"><name><text>m2</text></name></place><place id="m3"><name><text>m3</text></name></place><place id="x"><name><text>x</text></name></place><place id="y"><name><text>y</text></name></place><place id="z"><name><text>z</text></name></place><place id="w"><name><text>w</text></name></place><place id="g"><name><text>g</text></name></place><place id="pt"><name><text>pt</text></name></place><place id="sink"><name><text>sink</text></name></place><transition id="tS"><name><text>S</text></name></transition><transition id="b"><name><text>b</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="8b93efc6-7433-405f-83f5-98c6c5cf721b"/></transition><transition id="c"><name><text>c</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="07f252cb-c975-4c9f-9a58-693f1dfa06a2"/></transition><transition id="a"><name><text>a</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="39362a6b-bd4d-434f-b9be-6357222f19eb"/></transition><transition id="d"><name><text>d</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="b26bf79c-b088-4459-a9dd-09cefe14b3ee"/></transition><transition id="e"><name><text>e</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="14b8052e-f1f6-4b6a-9799-9474ddc3512b"/></transition><transition id="f"><name><text>f</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="cc41b0de-c0fc-4979-bb5b-19d881fe757f"/></transition><transition id="tT"><name><text>T</text></name></transition><arc id="140429583450640" source="m3" target="e"/><arc id="140429585414672" source="tS" target="m2"/><arc id="140429585414800" source="tS" target="m3"/><arc id="140429585414544" source="tS" target="m1"/><arc id="140429583450192" source="d" target="pt"/><arc id="140429583451152" source="g" target="f"/><arc id="140429583450512" source="w" target="f"/><arc id="140429583451280" source="f" target="pt"/><arc id="140429583451664" source="pt" target="tT"/><arc id="140429583450896" source="tT" target="sink"/><arc id="140429585416016" source="e" target="w"/><arc id="140429585415120" source="c" target="y"/><arc id="140429585414224" source="source" target="tS"/><arc id="140429585413968" source="m1" target="a"/><arc id="140429585414480" source="a" target="x"/><arc id="140429585414352" source="x" target="b"/><arc id="140429585415376" source="y" target="b"/><arc id="140429585415504" source="b" target="pt"/><arc id="140429585415760" source="m2" target="c"/><arc id="140429583450320" source="z" target="d"/><arc id="140429585415632" source="m2" target="d"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kTreeNet = R"PNML(<pnml><net id="imdf_net_1793665417.8079722" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><name><text>imdf_net_1793665417.8079722</text></name><page id="n0"><place id="p_17"><name><text>p_17</text></name></place><place id="p_15"><name><text>p_15</text></name></place><place id="sink"><name><text>sink</text></name></place><place id="p_6"><name><text>p_6</text></name></place><place id="p_7"><name><text>p_7</text></name></place><place id="p_9"><name><text>p_9</text></name></place><place id="p_11"><name><text>p_11</text></name></place><place id="p_13"><name><text>p_13</text></name></place><place id="p_14"><name><text>p_14</text></name></place><place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place><place id="p_5"><name><text>p_5</text></name></place><place id="p_10"><name><text>p_10</text></name></place><place id="p_18"><name><text>p_18</text></name></place><place id="p_12"><name><text>p_12</text></name></place><transition id="skip_9"><name><text>skip_9</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="8fe78bf7-af27-4ea6-8385-a2077e6d7ec2"/></transition><transition id="init_loop_15"><name><text>init_loop_15</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="ac4ea2fb-a585-4361-992d-538ad7916747"/></transition><transition id="skip_14"><name><text>skip_14</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="91a80c6c-5881-4eb5-9a9b-5d5a74a291ac"/></transition><transition id="d6246af2-8903-4add-971e-29ac353bab50"><name><text>g</text></name></transition><transition id="tauJoin_6"><name><text>tauJoin_6</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="d599a86d-c542-477c-880a-138096a1197c"/></transition><transition id="411a818b-b670-4316-bdc3-8550a3d98bac"><name><text>f</text></name></transition><transition id="skip_13"><name><text>skip_13</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="bf8fdfbf-b9a4-45aa-98fb-87671ff717ac"/></transition><transition id="skip_17"><name><text>skip_17</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="3019ad87-c5e4-46fc-b59b-8039a82fe234"/></transition><transition id="skip_19"><name><text>skip_19</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="68de3a15-049b-4c21-8fda-a8b4c2f3d602"/></transition><transition id="1cccdded-0202-4618-b8f3-af8e32638dc3"><name><text>c</text></name></transition><transition id="skip_12"><name><text>skip_12</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="bd866c99-4d5e-4bb2-b765-b364b328f412"/></transition><transition id="skip_8"><name><text>skip_8</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="27c7ab6d-2805-4d14-86fb-66d152cb5d2d"/></transition><transition id="init_loop_10"><name><text>init_loop_10</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="692ceea8-5607-4b6e-b82a-57c875e70079"/></transition><transition id="tau_1"><name><text>tau_1</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="26277b25-7437-4f02-9f7f-1ff27dc144f7"/></transition><transition id="6a0a0f11-d1df-4258-83b7-b96fb7fca12e"><name><text>h</text></name></transition><transition id="tauSplit_5"><name><text>tauSplit_5</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="7f21ea69-f4d1-405c-a28f-137f497b3761"/></transition><transition id="skip_7"><name><text>skip_7</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="a07b4aa2-4a00-418b-93ff-ee2d4c52799e"/></transition><transition id="1f343734-8d1c-45ae-b197-4f666d03318d"><name><text>e</text></name></transition><transition id="skip_18"><name><text>skip_18</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="70f498d7-82ac-45ea-9de9-5e010a9e4367"/></transition><arc id="139968950367760" source="1cccdded-0202-4618-b8f3-af8e32638dc3" target="p_18"/><arc id="139968950362064" source="p_11" target="skip_8"/><arc id="139968950403600" source="tauJoin_6" target="p_5"/><arc id="139968950360976" source="p_9" target="411a818b-b670-4316-bdc3-8550a3d98bac"/><arc id="139968950361104" source="411a818b-b670-4316-bdc3-8550a3d98bac" target="p_10"/><arc id="139968950360208" source="p_10" target="tauJoin_6"/><arc id="139968950361744" source="p_12" target="tauJoin_6"/><arc id="139968950362192" source="skip_8" target="p_12"/><arc id="139968950362768" source="p_11" target="skip_9"/><arc id="139968950362896" source="skip_9" target="p_13"/><arc id="139968950365584" source="p_15" target="skip_13"/><arc id="139968950365712" source="skip_13" target="p_13"/><arc id="139968950368272" source="p_18" target="skip_17"/><arc id="139968950358352" source="6a0a0f11-d1df-4258-83b7-b96fb7fca12e" target="p_7"/><arc id="139968950358608" source="p_7" target="d6246af2-8903-4add-971e-29ac353bab50"/><arc id="139968950358736" source="d6246af2-8903-4add-971e-29ac353bab50" target="p_6"/><arc id="139968950404176" source="skip_17" target="p_17"/><arc id="139968950363472" source="p_11" target="init_loop_10"/><arc id="139968950366224" source="p_13" target="skip_14"/><arc id="139968950363600" source="init_loop_10" target="p_14"/><arc id="139968950403152" source="skip_12" target="p_14"/><arc id="139968950360656" source="skip_7" target="p_10"/><arc id="139968950359248" source="p_6" target="tauSplit_5"/><arc id="139968950359888" source="tauSplit_5" target="p_9"/><arc id="139968950355792" source="source" target="tau_1"/><arc id="139968950405072" source="skip_19" target="sink"/><arc id="139968950405968" source="tau_1" target="p_5"/><arc id="139968950358224" source="p_5" target="6a0a0f11-d1df-4258-83b7-b96fb7fca12e"/><arc id="139968950364368" source="p_14" target="1f343734-8d1c-45ae-b197-4f666d03318d"/><arc id="139968950364496" source="1f343734-8d1c-45ae-b197-4f666d03318d" target="p_15"/><arc id="139968950366352" source="skip_14" target="p_12"/><arc id="139968950360528" source="p_9" target="skip_7"/><arc id="139968950368848" source="p_18" target="skip_18"/><arc id="139968950366864" source="p_13" target="init_loop_15"/><arc id="139968950368976" source="skip_18" target="p_12"/><arc id="139968950366992" source="init_loop_15" target="p_17"/><arc id="139968950365008" source="p_15" target="skip_12"/><arc id="139968950361424" source="tauSplit_5" target="p_11"/><arc id="139968950367632" source="p_17" target="1cccdded-0202-4618-b8f3-af8e32638dc3"/><arc id="139968950369744" source="p_6" target="skip_19"/></page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)PNML";
constexpr std::string_view kTreePtml = R"PTML(<ptml><processTree name="e7967c53-4cf4-45ed-b27e-ec49c9026305" root="55a957b4-d28f-4df1-988a-0c23973d57dd" id="07494ffe-2f25-4cf0-80a3-80fd55826fcd"><xorLoop name="" id="55a957b4-d28f-4df1-988a-0c23973d57dd"/><sequence name="" id="2e3c262d-37d2-4b6d-bef2-5adb8cb8ce59"/><and name="" id="2b427be2-e73f-4ac9-970a-68e8939708d4"/><automaticTask name="" id="8b8caecc-e011-4c98-9949-b5959ceb8a50"/><manualTask name="h" id="26ea1498-db34-4883-a098-463abeb6733b"/><manualTask name="g" id="b365610b-d041-4583-964e-43bee612a968"/><xor name="" id="e73e3868-73e8-44e2-bc59-27dd38e7f5ea"/><xor name="" id="d1ac5df5-bd7f-439b-9603-e2bb949cd6c2"/><automaticTask name="" id="fd8528b4-cbd0-4091-8052-12fb843e4fd6"/><manualTask name="f" id="470466b7-ed44-4792-958d-f9f678d4bbcb"/><automaticTask name="" id="180424f0-beb9-4268-b454-7b883def2e2a"/><sequence name="" id="ede6b74d-80da-4794-b0d1-8c9a0e6c914a"/><xor name="" id="c625557c-ceb7-4b82-b7bc-4e0ea7d939d7"/><xor name="" id="4e44d6f8-fcd6-4ca0-8036-dc13ce45de1a"/><automaticTask name="" id="64edec6c-2ee4-4488-9c16-cc2ec138acd7"/><xorLoop name="" id="45bac364-178b-433b-86d0-bf244298c4b3"/><automaticTask name="" id="a337e0c7-e134-4589-8c19-749a0ee76246"/><xorLoop name="" id="264ca0a1-b5be-4093-9cd1-04c23196a748"/><manualTask name="e" id="c320db28-a675-4974-a471-028e4acaf616"/><automaticTask name="" id="d42b48d1-c059-467d-b662-f50c76df4bf8"/><automaticTask name="" id="511ae853-38ad-4223-94b9-1c0a41315c70"/><manualTask name="c" id="368ab4d5-5525-491b-b325-a2b3cf44a801"/><automaticTask name="" id="65fbd77a-2928-4681-9a47-abf9b8fd48b2"/><automaticTask name="" id="7ec55fa2-0a94-40c9-b4a7-82bfab2a9667"/><parentsNode id="6bfdeac1-ba58-44f7-938d-f7388eaffc85" sourceId="55a957b4-d28f-4df1-988a-0c23973d57dd" targetId="2e3c262d-37d2-4b6d-bef2-5adb8cb8ce59"/><parentsNode id="e5036034-e869-4eaf-87cb-09985762004e" sourceId="55a957b4-d28f-4df1-988a-0c23973d57dd" targetId="2b427be2-e73f-4ac9-970a-68e8939708d4"/><parentsNode id="4ec493a3-726d-4f7c-a614-6862c64e3736" sourceId="55a957b4-d28f-4df1-988a-0c23973d57dd" targetId="8b8caecc-e011-4c98-9949-b5959ceb8a50"/><parentsNode id="0302a022-4c07-405a-925a-5067d1e5ae1e" sourceId="2e3c262d-37d2-4b6d-bef2-5adb8cb8ce59" targetId="26ea1498-db34-4883-a098-463abeb6733b"/><parentsNode id="5ff5fc1a-3d12-4e73-b61a-1462c80294ff" sourceId="2e3c262d-37d2-4b6d-bef2-5adb8cb8ce59" targetId="b365610b-d041-4583-964e-43bee612a968"/><parentsNode id="ad87e92e-3bcd-464d-8a71-f7076cfaef53" sourceId="2b427be2-e73f-4ac9-970a-68e8939708d4" targetId="e73e3868-73e8-44e2-bc59-27dd38e7f5ea"/><parentsNode id="b560347d-1ec4-4fa6-8c45-510f9c384c48" sourceId="2b427be2-e73f-4ac9-970a-68e8939708d4" targetId="d1ac5df5-bd7f-439b-9603-e2bb949cd6c2"/><parentsNode id="e2c99227-bfd8-427a-b4a6-808826360736" sourceId="e73e3868-73e8-44e2-bc59-27dd38e7f5ea" targetId="fd8528b4-cbd0-4091-8052-12fb843e4fd6"/><parentsNode id="1de92924-cf4a-463f-9f7b-a4dc21823604" sourceId="e73e3868-73e8-44e2-bc59-27dd38e7f5ea" targetId="470466b7-ed44-4792-958d-f9f678d4bbcb"/><parentsNode id="e2045c62-3992-47d8-8abf-0058dfef3711" sourceId="d1ac5df5-bd7f-439b-9603-e2bb949cd6c2" targetId="180424f0-beb9-4268-b454-7b883def2e2a"/><parentsNode id="87a99dfe-dc27-4a2c-b188-a5b1b6616b6a" sourceId="d1ac5df5-bd7f-439b-9603-e2bb949cd6c2" targetId="ede6b74d-80da-4794-b0d1-8c9a0e6c914a"/><parentsNode id="a879200a-1d32-4f15-825e-d0c082d0fbcf" sourceId="ede6b74d-80da-4794-b0d1-8c9a0e6c914a" targetId="c625557c-ceb7-4b82-b7bc-4e0ea7d939d7"/><parentsNode id="b7e6822f-756c-445a-86bd-6b2f545b46ca" sourceId="ede6b74d-80da-4794-b0d1-8c9a0e6c914a" targetId="4e44d6f8-fcd6-4ca0-8036-dc13ce45de1a"/><parentsNode id="55549ae9-5a85-4f87-855d-dd3c4a72f167" sourceId="c625557c-ceb7-4b82-b7bc-4e0ea7d939d7" targetId="64edec6c-2ee4-4488-9c16-cc2ec138acd7"/><parentsNode id="e33b5154-3b5d-4f4c-ac2c-425963b15642" sourceId="c625557c-ceb7-4b82-b7bc-4e0ea7d939d7" targetId="45bac364-178b-433b-86d0-bf244298c4b3"/><parentsNode id="db994d85-35ee-4a0f-9846-04a9103ce39d" sourceId="4e44d6f8-fcd6-4ca0-8036-dc13ce45de1a" targetId="a337e0c7-e134-4589-8c19-749a0ee76246"/><parentsNode id="176923dc-39b5-4131-a22d-ab43d1736d1b" sourceId="4e44d6f8-fcd6-4ca0-8036-dc13ce45de1a" targetId="264ca0a1-b5be-4093-9cd1-04c23196a748"/><parentsNode id="5bd5fde0-6a51-40d3-bc28-eacbae2af707" sourceId="45bac364-178b-433b-86d0-bf244298c4b3" targetId="c320db28-a675-4974-a471-028e4acaf616"/><parentsNode id="11938947-43bd-4065-be89-6b295a667948" sourceId="45bac364-178b-433b-86d0-bf244298c4b3" targetId="d42b48d1-c059-467d-b662-f50c76df4bf8"/><parentsNode id="993a88ca-1226-4b15-99d8-18f520bb1eba" sourceId="45bac364-178b-433b-86d0-bf244298c4b3" targetId="511ae853-38ad-4223-94b9-1c0a41315c70"/><parentsNode id="d3992d29-1aca-40cd-b204-62a94b821197" sourceId="264ca0a1-b5be-4093-9cd1-04c23196a748" targetId="368ab4d5-5525-491b-b325-a2b3cf44a801"/><parentsNode id="f12d334b-af1d-434d-90af-891b531c1bb7" sourceId="264ca0a1-b5be-4093-9cd1-04c23196a748" targetId="65fbd77a-2928-4681-9a47-abf9b8fd48b2"/><parentsNode id="02a52903-54f5-4da9-8b43-a018ed077ce5" sourceId="264ca0a1-b5be-4093-9cd1-04c23196a748" targetId="7ec55fa2-0a94-40c9-b4a7-82bfab2a9667"/></processTree></ptml>)PTML";

minera::EventLog log_from_variants(const std::vector<std::vector<std::string>>& variants) {
  minera::EventLog log;
  std::size_t case_index = 0;
  for (const auto& variant : variants) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& activity : variant) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool near(double actual, double expected) { return std::abs(actual - expected) < 1e-12; }

// Replay options as the PNML document entry points build them.
minera::TokenReplayOptions document_options(const minera::PnmlDocument& document) {
  minera::TokenReplayOptions options;
  options.exact_initial_marking = minera::TokenMarking(document.initial_marking);
  options.exact_final_marking = minera::TokenMarking(document.final_marking);
  options.initial_marking_keys = document.initial_marking_keys;
  options.final_marking_keys = document.final_marking_keys;
  options.place_names = document.place_ids;
  options.transition_names = document.transition_ids;
  options.place_outgoing_transition_order = minera::pnml_place_outgoing_transition_order(document);
  return options;
}

std::vector<std::string> fired_ids(const minera::PnmlDocument& document,
                                   const minera::TokenReplayTraceDiagnostics& row) {
  std::vector<std::string> ids;
  for (const auto transition : row.activated_transitions)
    ids.push_back(document.transition_ids.at(transition));
  return ids;
}

struct Counts { int produced, consumed, missing, remaining; };

void expect_counts(const minera::TokenReplayTraceDiagnostics& row, Counts expected) {
  assert(row.produced == expected.produced);
  assert(row.consumed == expected.consumed);
  assert(row.missing == expected.missing);
  assert(row.remaining == expected.remaining);
}

double dashboard_precision(const minera::EventLog& log, const minera::PnmlDocument& document) {
  const auto result = minera::check_conformance(log, document.net, document_options(document));
  assert(result.precision_computed);
  return result.precision;
}

double evaluator_precision(const minera::EventLog& log, const minera::PnmlDocument& document) {
  const auto result = minera::evaluate_precision(
      log, document, minera::PrecisionVariant::ETCONFORMANCE_TOKEN);
  assert(result.computed);
  return result.precision;
}

// Silent a_w (p2 -> p4) sorts before b_t (p1 -> p3): the eventually-enabled
// worklist fires b_t only from the marking a_w reached, so A (p3 + p2) is never
// reported after S. Renaming a_w to z_w reverses the order and reports A.
void eventually_enabled_worklist_follows_names() {
  const auto log = log_from_variants({{"S", "B"}});
  const auto first = minera::parse_pnml_document(kM1);
  assert(near(dashboard_precision(log, first), 1.0));
  assert(near(evaluator_precision(log, first), 1.0));
  const auto renamed = minera::parse_pnml_document(kM1Renamed);
  assert(near(dashboard_precision(log, renamed), 2.0 / 3.0));
  assert(near(evaluator_precision(log, renamed), 2.0 / 3.0));
}

// The empty prefix uses the same worklist: after the silent split, A is not
// reported at the initial marking (exact reachability would give 0.5).
void empty_prefix_uses_the_worklist() {
  const auto log = log_from_variants({{"B"}});
  const auto document = minera::parse_pnml_document(kM1Root);
  assert(near(dashboard_precision(log, document), 1.0));
  assert(near(evaluator_precision(log, document), 1.0));
}

// The reference joins prefix labels with commas and splits them again, so the
// single event "a,b" replays as a then b.
void comma_labels_split_into_prefix_events() {
  const minera::ProcessTree tree({"a", "b", "c"}, minera::ProcessTreeNode{
      minera::ProcessTreeOperator::kSequence, 0,
      {minera::ProcessTreeNode::leaf(0), minera::ProcessTreeNode::leaf(1),
       minera::ProcessTreeNode::leaf(2)}});
  const auto log = log_from_variants({{"a,b", "c"}});
  const auto net = minera::to_petri_net(tree);
  assert(near(minera::check_conformance(log, net, {}).precision, 0.5));
  assert(near(minera::evaluate_precision(
      log, net, minera::PrecisionVariant::ETCONFORMANCE_TOKEN).precision, 0.5));
}

// After recursion enables T through a, c, b, the second routing pass keeps
// going: e and f still fire although T is already enabled.
void second_pass_does_not_stop_when_enabled() {
  const auto document = minera::parse_pnml_document(kM3);
  const auto rows = minera::replay_token_traces(
      log_from_variants({{"S", "T"}}), document.net, document_options(document));
  expect_counts(rows.at(0), {10, 9, 0, 1});
  assert((fired_ids(document, rows.at(0)) ==
          std::vector<std::string>{"tS", "a", "c", "b", "e", "f", "tT"}));
}

// The extra firings reach precision: f consumes w, so U is not enabled after
// S, T in the reference (an early stop would leave w marked and report U).
void second_pass_firings_reach_precision() {
  const auto document = minera::parse_pnml_document(kM4);
  const auto log = log_from_variants({{"S", "T", "T"}});
  const auto rows = minera::replay_token_traces(log, document.net, document_options(document));
  expect_counts(rows.at(0), {11, 10, 0, 1});
  assert(near(dashboard_precision(log, document), 0.75));
  assert(near(evaluator_precision(log, document), 0.75));
}

// Exhaustive exploration shares the second pass: when the transactional trial
// fails (f also needs g), the pass fires e after b has enabled T.
void exhaustive_second_pass_does_not_stop() {
  const auto document = minera::parse_pnml_document(kM5);
  auto options = document_options(document);
  options.exhaustive_invisible_exploration = true;
  const auto rows = minera::replay_token_traces(log_from_variants({{"S", "T"}}), document.net, options);
  expect_counts(rows.at(0), {9, 8, 0, 1});
  assert((fired_ids(document, rows.at(0)) ==
          std::vector<std::string>{"tS", "a", "c", "b", "e", "tT"}));
}

// *(->(h, g), +(X(tau, f), X(tau, ->(X(tau, *(e, tau)), X(tau, *(c, tau))))))
// with trace g g f h: at h the reference also fires tau_1, tauSplit_5 and
// skip_7 after skip_8 and tauJoin_6 have enabled h.
void tree_net_document_replay() {
  const auto document = minera::parse_pnml_document(kTreeNet);
  const auto rows = minera::replay_token_traces(
      log_from_variants({{"g", "g", "f", "h"}}), document.net, document_options(document));
  expect_counts(rows.at(0), {13, 12, 3, 4});
  const auto ids = fired_ids(document, rows.at(0));
  const std::vector<std::string> silent(ids.begin() + 2, ids.end());
  assert((std::vector<std::string>{silent[0], silent[2], silent[3], silent[4], silent[5],
                                   silent[6]} ==
          std::vector<std::string>{"tauSplit_5", "skip_8", "tauJoin_6", "tau_1", "tauSplit_5",
                                   "skip_7"}));
}

// The same tree converted in memory carries the reference place names, so
// replay without any document options sorts routing candidates the same way
// (positional p<index> names would give 9/8/2/3).
void converted_tree_carries_place_names() {
  const auto tree = minera::parse_ptml(kTreePtml);
  const auto net = minera::to_petri_net(tree);
  assert(net.place_names());
  const auto& names = *net.place_names();
  assert(names.size() == net.place_count());
  assert(names.at(net.source()) == "source" && names.at(net.sink()) == "sink");
  const auto log = log_from_variants({{"g", "g", "f", "h"}});
  const auto result = minera::check_conformance(log, net, {});
  assert(result.produced == 13 && result.consumed == 12);
  assert(result.missing == 3 && result.remaining == 4);

  // Copies and a PNML round trip keep the names.
  const minera::PetriNet copy = net;
  assert(copy.place_names() == net.place_names());
  const auto reread = minera::parse_pnml_document(minera::to_pnml(net));
  assert(reread.place_ids == names);
  assert(reread.net.place_names() && *reread.net.place_names() == names);
  const auto again = minera::check_conformance(log, reread.net, {});
  assert(again.produced == 13 && again.consumed == 12);
}

}  // namespace

int main() {
  eventually_enabled_worklist_follows_names();
  empty_prefix_uses_the_worklist();
  comma_labels_split_into_prefix_events();
  second_pass_does_not_stop_when_enabled();
  second_pass_firings_reach_precision();
  exhaustive_second_pass_does_not_stop();
  tree_net_document_replay();
  converted_tree_carries_place_names();
  return 0;
}
