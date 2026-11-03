// Generated from pm4py 2.7.22 convert_to_petri_net(read_bpmn(...)) on these inputs.
// Fields: name, BPMN, places, transitions (name|label|inputs|outputs, U = random UUID), initial, final.
{"seq.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s" name="S"><outgoing>f0</outgoing></startEvent>
<task id="a" name="A"><incoming>f0</incoming><outgoing>f1</outgoing></task>
<task id="b" name="B"><incoming>f1</incoming><outgoing>f2</outgoing></task>
<endEvent id="e" name="E"><incoming>f2</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="a"/>
<sequenceFlow id="f1" sourceRef="a" targetRef="b"/>
<sequenceFlow id="f2" sourceRef="b" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_b", "sink", "source"},
{"a|A|source|ent_b", "b|B|ent_b|sink"},
{"source"}, {"sink"}},
{"xor.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<exclusiveGateway id="x1"><incoming>f0</incoming><outgoing>f1</outgoing><outgoing>f2</outgoing></exclusiveGateway>
<task id="a" name="A"><incoming>f1</incoming><outgoing>f3</outgoing></task>
<task id="b" name="B"><incoming>f2</incoming><outgoing>f4</outgoing></task>
<exclusiveGateway id="x2"><incoming>f3</incoming><incoming>f4</incoming><outgoing>f5</outgoing></exclusiveGateway>
<endEvent id="e"><incoming>f5</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="x1"/>
<sequenceFlow id="f1" sourceRef="x1" targetRef="a"/>
<sequenceFlow id="f2" sourceRef="x1" targetRef="b"/>
<sequenceFlow id="f3" sourceRef="a" targetRef="x2"/>
<sequenceFlow id="f4" sourceRef="b" targetRef="x2"/>
<sequenceFlow id="f5" sourceRef="x2" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_x2", "exi_x1", "sink", "source"},
{"U|~|source|exi_x1", "a|A|exi_x1|ent_x2", "b|B|exi_x1|ent_x2", "x2|~|ent_x2|sink"},
{"source"}, {"sink"}},
{"and.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<parallelGateway id="x1"><incoming>f0</incoming><outgoing>f1</outgoing><outgoing>f2</outgoing></parallelGateway>
<task id="a" name="A"><incoming>f1</incoming><outgoing>f3</outgoing></task>
<task id="b" name="B"><incoming>f2</incoming><outgoing>f4</outgoing></task>
<parallelGateway id="x2"><incoming>f3</incoming><incoming>f4</incoming><outgoing>f5</outgoing></parallelGateway>
<endEvent id="e"><incoming>f5</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="x1"/>
<sequenceFlow id="f1" sourceRef="x1" targetRef="a"/>
<sequenceFlow id="f2" sourceRef="x1" targetRef="b"/>
<sequenceFlow id="f3" sourceRef="a" targetRef="x2"/>
<sequenceFlow id="f4" sourceRef="b" targetRef="x2"/>
<sequenceFlow id="f5" sourceRef="x2" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_a", "ent_b", "f3", "f4", "sink", "source"},
{"U|~|f3,f4|sink", "U|~|source|ent_a,ent_b", "a|A|ent_a|f3", "b|B|ent_b|f4"},
{"source"}, {"sink"}},
{"two_start.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s1"><outgoing>f0</outgoing></startEvent>
<startEvent id="s2"><outgoing>f1</outgoing></startEvent>
<task id="a" name="A"><incoming>f0</incoming><incoming>f1</incoming><outgoing>f2</outgoing></task>
<endEvent id="e"><incoming>f2</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s1" targetRef="a"/>
<sequenceFlow id="f1" sourceRef="s2" targetRef="a"/>
<sequenceFlow id="f2" sourceRef="a" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_a", "sink", "source"},
{"U|~|source|ent_a", "U|~|source|ent_a", "a|A|ent_a|sink"},
{"source"}, {"sink"}},
{"iso.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<task id="a" name="A"><incoming>f0</incoming><outgoing>f1</outgoing></task>
<endEvent id="e"><incoming>f1</incoming></endEvent>
<task id="z" name="Z"></task>
<sequenceFlow id="f0" sourceRef="s" targetRef="a"/>
<sequenceFlow id="f1" sourceRef="a" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_z", "exi_z", "sink", "source"},
{"a|A|source|sink", "z|Z|ent_z|exi_z"},
{"source"}, {"sink"}},
{"noend.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<task id="a" name="A"><incoming>f0</incoming></task>
<sequenceFlow id="f0" sourceRef="s" targetRef="a"/>
</process>
</definitions>)BPMN",
{"exi_a", "sink", "source"},
{"a|A|source|exi_a"},
{"source"}, {"sink"}},
{"nostart.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<task id="a" name="A"><outgoing>f0</outgoing></task>
<endEvent id="e"><incoming>f0</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="a" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_a", "sink", "source"},
{"a|A|ent_a|sink"},
{"source"}, {"sink"}},
{"unnamedtask.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<task id="a"><incoming>f0</incoming><outgoing>f1</outgoing></task>
<endEvent id="e"><incoming>f1</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="a"/>
<sequenceFlow id="f1" sourceRef="a" targetRef="e"/>
</process>
</definitions>)BPMN",
{"sink", "source"},
{"U|~|source|sink"},
{"source"}, {"sink"}},
{"two_out_task.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<task id="a" name="A"><incoming>f0</incoming><outgoing>f1</outgoing><outgoing>f2</outgoing></task>
<task id="b" name="B"><incoming>f1</incoming><outgoing>f3</outgoing></task>
<task id="c" name="C"><incoming>f2</incoming><outgoing>f4</outgoing></task>
<endEvent id="e"><incoming>f3</incoming><incoming>f4</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="a"/>
<sequenceFlow id="f1" sourceRef="a" targetRef="b"/>
<sequenceFlow id="f2" sourceRef="a" targetRef="c"/>
<sequenceFlow id="f3" sourceRef="b" targetRef="e"/>
<sequenceFlow id="f4" sourceRef="c" targetRef="e"/>
</process>
</definitions>)BPMN",
{"ent_e", "exi_a", "sink", "source"},
{"a|A|source|exi_a", "b|B|exi_a|ent_e", "c|C|exi_a|ent_e", "e|~|ent_e|sink"},
{"source"}, {"sink"}},
{"and11.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<parallelGateway id="x1"><incoming>f0</incoming><outgoing>f1</outgoing></parallelGateway>
<task id="a" name="A"><incoming>f1</incoming><outgoing>f2</outgoing></task>
<endEvent id="e"><incoming>f2</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="x1"/>
<sequenceFlow id="f1" sourceRef="x1" targetRef="a"/>
<sequenceFlow id="f2" sourceRef="a" targetRef="e"/>
</process>
</definitions>)BPMN",
{"sink", "source"},
{"a|A|source|sink"},
{"source"}, {"sink"}},
{"two_end.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s"><outgoing>f0</outgoing></startEvent>
<exclusiveGateway id="x"><incoming>f0</incoming><outgoing>f1</outgoing><outgoing>f2</outgoing></exclusiveGateway>
<task id="a" name="A"><incoming>f1</incoming><outgoing>f3</outgoing></task>
<task id="b" name="B"><incoming>f2</incoming><outgoing>f4</outgoing></task>
<endEvent id="e1"><incoming>f3</incoming></endEvent>
<endEvent id="e2"><incoming>f4</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s" targetRef="x"/>
<sequenceFlow id="f1" sourceRef="x" targetRef="a"/>
<sequenceFlow id="f2" sourceRef="x" targetRef="b"/>
<sequenceFlow id="f3" sourceRef="a" targetRef="e1"/>
<sequenceFlow id="f4" sourceRef="b" targetRef="e2"/>
</process>
</definitions>)BPMN",
{"exi_x", "sink", "source"},
{"U|~|source|exi_x", "a|A|exi_x|sink", "b|B|exi_x|sink"},
{"source"}, {"sink"}},
{"r2_35.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<task id="n5" name="A4"><incoming>f8</incoming><outgoing>f8</outgoing></task>
<task id="n3" name="A1"><incoming>f0</incoming><incoming>f4</incoming><outgoing>f5</outgoing></task>
<parallelGateway id="n2"><incoming>f2</incoming><outgoing>f4</outgoing></parallelGateway>
<task id="n0" name="A3"><incoming>f1</incoming><incoming>f7</incoming><outgoing>f2</outgoing></task>
<parallelGateway id="n1"><outgoing>f3</outgoing></parallelGateway>
<startEvent id="s1"><outgoing>f1</outgoing></startEvent>
<endEvent id="e0"></endEvent>
<task id="n4" name="A3"><incoming>f3</incoming><outgoing>f6</outgoing><outgoing>f7</outgoing></task>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<endEvent id="e1"><incoming>f5</incoming><incoming>f6</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n3"/>
<sequenceFlow id="f1" sourceRef="s1" targetRef="n0"/>
<sequenceFlow id="f2" sourceRef="n0" targetRef="n2"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="n4"/>
<sequenceFlow id="f4" sourceRef="n2" targetRef="n3"/>
<sequenceFlow id="f5" sourceRef="n3" targetRef="e1"/>
<sequenceFlow id="f6" sourceRef="n4" targetRef="e1"/>
<sequenceFlow id="f7" sourceRef="n4" targetRef="n0"/>
<sequenceFlow id="f8" sourceRef="n5" targetRef="n5"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_e1", "ent_n0", "ent_n1", "ent_n3", "ent_n5", "exi_n4", "sink", "source"},
{"U|~|source|ent_n0", "U|~|source|ent_n3", "e0|~|ent_e0|sink", "e1|~|ent_e1|sink", "n0|A3|ent_n0|ent_n3", "n3|A1|ent_n3|ent_e1", "n4|A3|ent_n1|exi_n4", "n5|A4|ent_n5|ent_n5", "sfl_f6|~|exi_n4|ent_e1", "sfl_f7|~|exi_n4|ent_n0"},
{"source"}, {"sink"}},
{"r1_139.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<endEvent id="e0"></endEvent>
<parallelGateway id="n4"><outgoing>f10</outgoing><outgoing>f11</outgoing></parallelGateway>
<task id="n0" name="A2"><incoming>f10</incoming><outgoing>f1</outgoing></task>
<parallelGateway id="n6"><incoming>f1</incoming><incoming>f2</incoming><outgoing>f13</outgoing></parallelGateway>
<parallelGateway id="n3"><incoming>f4</incoming><outgoing>f7</outgoing><outgoing>f8</outgoing><outgoing>f9</outgoing></parallelGateway>
<task id="n2" name="A0"><incoming>f8</incoming><incoming>f11</incoming><outgoing>f5</outgoing><outgoing>f6</outgoing></task>
<parallelGateway id="n1"><incoming>f0</incoming><incoming>f6</incoming><incoming>f9</incoming><outgoing>f2</outgoing><outgoing>f3</outgoing><outgoing>f4</outgoing></parallelGateway>
<task id="n5" name="A0"><incoming>f7</incoming><incoming>f13</incoming><outgoing>f12</outgoing></task>
<endEvent id="e1"><incoming>f3</incoming><incoming>f5</incoming><incoming>f12</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n1"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="n6"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="n6"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="e1"/>
<sequenceFlow id="f4" sourceRef="n1" targetRef="n3"/>
<sequenceFlow id="f5" sourceRef="n2" targetRef="e1"/>
<sequenceFlow id="f6" sourceRef="n2" targetRef="n1"/>
<sequenceFlow id="f7" sourceRef="n3" targetRef="n5"/>
<sequenceFlow id="f8" sourceRef="n3" targetRef="n2"/>
<sequenceFlow id="f9" sourceRef="n3" targetRef="n1"/>
<sequenceFlow id="f10" sourceRef="n4" targetRef="n0"/>
<sequenceFlow id="f11" sourceRef="n4" targetRef="n2"/>
<sequenceFlow id="f12" sourceRef="n5" targetRef="e1"/>
<sequenceFlow id="f13" sourceRef="n6" targetRef="n5"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_e1", "ent_n0", "ent_n2", "ent_n4", "ent_n5", "exi_n2", "f1", "f2", "f9", "sink", "source"},
{"U|~|exi_n2,f9,source|ent_e1,ent_n2,ent_n5,f2,f9", "U|~|f1,f2|ent_n5", "e0|~|ent_e0|sink", "e1|~|ent_e1|sink", "n0|A2|ent_n0|f1", "n2|A0|ent_n2|exi_n2", "n4|~|ent_n4|ent_n0,ent_n2", "n5|A0|ent_n5|ent_e1", "sfl_f5|~|exi_n2|ent_e1"},
{"source"}, {"sink"}},
{"r1_275.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<task id="n1" name="A4"><incoming>f1</incoming><incoming>f5</incoming><outgoing>f2</outgoing></task>
<exclusiveGateway id="n7"><outgoing>f9</outgoing></exclusiveGateway>
<parallelGateway id="n4"><outgoing>f5</outgoing><outgoing>f6</outgoing></parallelGateway>
<task id="n0" name="A0"><outgoing>f1</outgoing></task>
<exclusiveGateway id="n3"><incoming>f7</incoming><outgoing>f4</outgoing></exclusiveGateway>
<endEvent id="e1"></endEvent>
<task id="n2" name="A2"><incoming>f0</incoming><incoming>f3</incoming><incoming>f4</incoming><incoming>f9</incoming><outgoing>f3</outgoing></task>
<task id="n6" name="A3"><incoming>f2</incoming><incoming>f8</incoming><outgoing>f8</outgoing></task>
<task id="n5" name="A4"><incoming>f6</incoming><outgoing>f7</outgoing></task>
<endEvent id="e0"></endEvent>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n2"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="n1"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="n6"/>
<sequenceFlow id="f3" sourceRef="n2" targetRef="n2"/>
<sequenceFlow id="f4" sourceRef="n3" targetRef="n2"/>
<sequenceFlow id="f5" sourceRef="n4" targetRef="n1"/>
<sequenceFlow id="f6" sourceRef="n4" targetRef="n5"/>
<sequenceFlow id="f7" sourceRef="n5" targetRef="n3"/>
<sequenceFlow id="f8" sourceRef="n6" targetRef="n6"/>
<sequenceFlow id="f9" sourceRef="n7" targetRef="n2"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_e1", "ent_n0", "ent_n1", "ent_n2", "ent_n4", "ent_n5", "ent_n6", "ent_n7", "sink", "source"},
{"U|~|source|ent_n2", "e0|~|ent_e0|sink", "e1|~|ent_e1|sink", "n0|A0|ent_n0|ent_n1", "n1|A4|ent_n1|ent_n6", "n2|A2|ent_n2|ent_n2", "n4|~|ent_n4|ent_n1,ent_n5", "n5|A4|ent_n5|ent_n2", "n6|A3|ent_n6|ent_n6", "n7|~|ent_n7|ent_n2"},
{"source"}, {"sink"}},
{"r1_243.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<exclusiveGateway id="n0"><incoming>f4</incoming><incoming>f7</incoming><outgoing>f1</outgoing></exclusiveGateway>
<endEvent id="e0"></endEvent>
<task id="n2" name="A3"><incoming>f0</incoming><incoming>f1</incoming><incoming>f3</incoming><outgoing>f5</outgoing></task>
<parallelGateway id="n4"><incoming>f2</incoming><incoming>f6</incoming><incoming>f8</incoming><outgoing>f7</outgoing><outgoing>f8</outgoing></parallelGateway>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<task id="n1" name="A2"><incoming>f5</incoming><outgoing>f2</outgoing><outgoing>f3</outgoing><outgoing>f4</outgoing></task>
<task id="n3"><outgoing>f6</outgoing></task>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n2"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="n2"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="n4"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="n2"/>
<sequenceFlow id="f4" sourceRef="n1" targetRef="n0"/>
<sequenceFlow id="f5" sourceRef="n2" targetRef="n1"/>
<sequenceFlow id="f6" sourceRef="n3" targetRef="n4"/>
<sequenceFlow id="f7" sourceRef="n4" targetRef="n0"/>
<sequenceFlow id="f8" sourceRef="n4" targetRef="n4"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_n0", "ent_n1", "ent_n2", "ent_n3", "exi_n1", "f8", "sink", "source"},
{"U|~|ent_n3,exi_n1,f8|ent_n0,f8", "U|~|source|ent_n2", "e0|~|ent_e0|sink", "n0|~|ent_n0|ent_n2", "n1|A2|ent_n1|exi_n1", "n2|A3|ent_n2|ent_n1", "sfl_f3|~|exi_n1|ent_n2", "sfl_f4|~|exi_n1|ent_n0"},
{"source"}, {"sink"}},
{"r2_232.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<task id="n0" name="A2"><incoming>f5</incoming><outgoing>f2</outgoing></task>
<startEvent id="s1"><outgoing>f1</outgoing></startEvent>
<endEvent id="e1"><incoming>f4</incoming><incoming>f6</incoming></endEvent>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<endEvent id="e0"></endEvent>
<parallelGateway id="n3"><incoming>f0</incoming><incoming>f2</incoming><outgoing>f6</outgoing></parallelGateway>
<exclusiveGateway id="n1"><outgoing>f3</outgoing><outgoing>f4</outgoing></exclusiveGateway>
<task id="n2" name="A0"><incoming>f1</incoming><incoming>f3</incoming><outgoing>f5</outgoing></task>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n3"/>
<sequenceFlow id="f1" sourceRef="s1" targetRef="n2"/>
<sequenceFlow id="f2" sourceRef="n0" targetRef="n3"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="n2"/>
<sequenceFlow id="f4" sourceRef="n1" targetRef="e1"/>
<sequenceFlow id="f5" sourceRef="n2" targetRef="n0"/>
<sequenceFlow id="f6" sourceRef="n3" targetRef="e1"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_e1", "ent_n0", "ent_n1", "ent_n2", "exi_n1", "f2", "sink", "source"},
{"U|~|f2,source|ent_e1", "U|~|source|ent_n2", "e0|~|ent_e0|sink", "e1|~|ent_e1|sink", "n0|A2|ent_n0|f2", "n1|~|ent_n1|exi_n1", "n2|A0|ent_n2|ent_n0", "sfl_f3|~|exi_n1|ent_n2", "sfl_f4|~|exi_n1|ent_e1"},
{"source"}, {"sink"}},
{"r1_27.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<parallelGateway id="n3"><incoming>f5</incoming><incoming>f7</incoming><outgoing>f4</outgoing><outgoing>f5</outgoing><outgoing>f6</outgoing></parallelGateway>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<task id="n4" name="A3"><incoming>f0</incoming><incoming>f3</incoming><outgoing>f7</outgoing></task>
<task id="n1" name="A4"><outgoing>f2</outgoing></task>
<task id="n2" name="A4"><outgoing>f3</outgoing></task>
<task id="n0" name="A3"><outgoing>f1</outgoing></task>
<endEvent id="e1"><incoming>f1</incoming><incoming>f4</incoming><incoming>f6</incoming></endEvent>
<endEvent id="e0"><incoming>f2</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n4"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="e1"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="e0"/>
<sequenceFlow id="f3" sourceRef="n2" targetRef="n4"/>
<sequenceFlow id="f4" sourceRef="n3" targetRef="e1"/>
<sequenceFlow id="f5" sourceRef="n3" targetRef="n3"/>
<sequenceFlow id="f6" sourceRef="n3" targetRef="e1"/>
<sequenceFlow id="f7" sourceRef="n4" targetRef="n3"/>
</process>
</definitions>)BPMN",
{"ent_e1", "ent_n0", "ent_n1", "ent_n2", "ent_n4", "f5", "f7", "sink", "source"},
{"U|~|f5,f7|ent_e1,ent_e1,f5", "U|~|source|ent_n4", "e1|~|ent_e1|sink", "n0|A3|ent_n0|ent_e1", "n1|A4|ent_n1|sink", "n2|A4|ent_n2|ent_n4", "n4|A3|ent_n4|f7"},
{"source"}, {"sink"}},
{"r2_175.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<exclusiveGateway id="n4"><outgoing>f6</outgoing></exclusiveGateway>
<startEvent id="s0"><outgoing>f0</outgoing><outgoing>f1</outgoing></startEvent>
<endEvent id="e0"><incoming>f1</incoming></endEvent>
<exclusiveGateway id="n2"><incoming>f5</incoming><outgoing>f4</outgoing></exclusiveGateway>
<task id="n3" name="A1"><incoming>f3</incoming><incoming>f4</incoming><outgoing>f5</outgoing></task>
<exclusiveGateway id="n0"><outgoing>f2</outgoing></exclusiveGateway>
<parallelGateway id="n1"><incoming>f2</incoming><incoming>f6</incoming><outgoing>f3</outgoing></parallelGateway>
<task id="n5" name="A2"><incoming>f0</incoming><incoming>f7</incoming><outgoing>f7</outgoing></task>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n5"/>
<sequenceFlow id="f1" sourceRef="s0" targetRef="e0"/>
<sequenceFlow id="f2" sourceRef="n0" targetRef="n1"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="n3"/>
<sequenceFlow id="f4" sourceRef="n2" targetRef="n3"/>
<sequenceFlow id="f5" sourceRef="n3" targetRef="n2"/>
<sequenceFlow id="f6" sourceRef="n4" targetRef="n1"/>
<sequenceFlow id="f7" sourceRef="n5" targetRef="n5"/>
</process>
</definitions>)BPMN",
{"ent_n0", "ent_n3", "ent_n4", "ent_n5", "exi_s0", "sink", "source"},
{"U|~|ent_n0,ent_n4|ent_n3", "U|~|source|exi_s0", "n3|A1|ent_n3|ent_n3", "n5|A2|ent_n5|ent_n5", "sfl_f0|~|exi_s0|ent_n5", "sfl_f1|~|exi_s0|sink"},
{"source"}, {"sink"}},
{"r2_217.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<parallelGateway id="n0"><outgoing>f1</outgoing></parallelGateway>
<parallelGateway id="n4"><outgoing>f5</outgoing><outgoing>f6</outgoing></parallelGateway>
<task id="n2" name="A4"><outgoing>f3</outgoing></task>
<parallelGateway id="n5"><incoming>f5</incoming><incoming>f7</incoming><incoming>f11</incoming><outgoing>f7</outgoing><outgoing>f8</outgoing><outgoing>f9</outgoing></parallelGateway>
<task id="n1"><incoming>f0</incoming><incoming>f4</incoming><incoming>f8</incoming><outgoing>f2</outgoing></task>
<exclusiveGateway id="n6"><incoming>f10</incoming><incoming>f12</incoming><outgoing>f10</outgoing></exclusiveGateway>
<task id="n7" name="A4"><incoming>f9</incoming><outgoing>f11</outgoing></task>
<endEvent id="e0"></endEvent>
<exclusiveGateway id="n8"><outgoing>f12</outgoing></exclusiveGateway>
<endEvent id="e1"><incoming>f1</incoming><incoming>f2</incoming><incoming>f3</incoming></endEvent>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<task id="n3" name="A1"><incoming>f6</incoming><outgoing>f4</outgoing></task>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n1"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="e1"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="e1"/>
<sequenceFlow id="f3" sourceRef="n2" targetRef="e1"/>
<sequenceFlow id="f4" sourceRef="n3" targetRef="n1"/>
<sequenceFlow id="f5" sourceRef="n4" targetRef="n5"/>
<sequenceFlow id="f6" sourceRef="n4" targetRef="n3"/>
<sequenceFlow id="f7" sourceRef="n5" targetRef="n5"/>
<sequenceFlow id="f8" sourceRef="n5" targetRef="n1"/>
<sequenceFlow id="f9" sourceRef="n5" targetRef="n7"/>
<sequenceFlow id="f10" sourceRef="n6" targetRef="n6"/>
<sequenceFlow id="f11" sourceRef="n7" targetRef="n5"/>
<sequenceFlow id="f12" sourceRef="n8" targetRef="n6"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_e1", "ent_n0", "ent_n1", "ent_n2", "ent_n3", "ent_n4", "ent_n6", "ent_n7", "ent_n8", "f11", "f5", "f7", "sink", "source"},
{"U|~|f11,f5,f7|ent_n1,ent_n7,f7", "U|~|source|ent_n1", "e0|~|ent_e0|sink", "e1|~|ent_e1|sink", "n0|~|ent_n0|ent_e1", "n1|~|ent_n1|ent_e1", "n2|A4|ent_n2|ent_e1", "n3|A1|ent_n3|ent_n1", "n4|~|ent_n4|ent_n3,f5", "n6|~|ent_n6|ent_n6", "n7|A4|ent_n7|f11", "n8|~|ent_n8|ent_n6"},
{"source"}, {"sink"}},
{"r1_44.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<parallelGateway id="n1"><outgoing>f2</outgoing><outgoing>f3</outgoing></parallelGateway>
<task id="n2" name="A1"><incoming>f0</incoming><incoming>f2</incoming><incoming>f3</incoming><incoming>f4</incoming><outgoing>f4</outgoing></task>
<endEvent id="e0"><incoming>f1</incoming></endEvent>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<task id="n0" name="A3"><outgoing>f1</outgoing></task>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n2"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="e0"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="n2"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="n2"/>
<sequenceFlow id="f4" sourceRef="n2" targetRef="n2"/>
</process>
</definitions>)BPMN",
{"ent_n0", "ent_n1", "ent_n2", "sink", "source"},
{"U|~|source|ent_n2", "n0|A3|ent_n0|sink", "n1|~|ent_n1|ent_n2,ent_n2", "n2|A1|ent_n2|ent_n2"},
{"source"}, {"sink"}},
{"r2_97.bpmn",
R"BPMN(<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" id="d" targetNamespace="x">
<process id="P">
<task id="n2" name="A3"><incoming>f0</incoming><outgoing>f5</outgoing></task>
<task id="n3" name="A2"><outgoing>f6</outgoing></task>
<startEvent id="s0"><outgoing>f0</outgoing></startEvent>
<task id="n5" name="A1"><incoming>f6</incoming><outgoing>f8</outgoing></task>
<exclusiveGateway id="n4"><incoming>f8</incoming><outgoing>f7</outgoing></exclusiveGateway>
<parallelGateway id="n1"><incoming>f4</incoming><incoming>f5</incoming><incoming>f7</incoming><outgoing>f2</outgoing><outgoing>f3</outgoing><outgoing>f4</outgoing></parallelGateway>
<task id="n0" name="A0"><incoming>f2</incoming><outgoing>f1</outgoing></task>
<endEvent id="e0"><incoming>f1</incoming><incoming>f3</incoming></endEvent>
<sequenceFlow id="f0" sourceRef="s0" targetRef="n2"/>
<sequenceFlow id="f1" sourceRef="n0" targetRef="e0"/>
<sequenceFlow id="f2" sourceRef="n1" targetRef="n0"/>
<sequenceFlow id="f3" sourceRef="n1" targetRef="e0"/>
<sequenceFlow id="f4" sourceRef="n1" targetRef="n1"/>
<sequenceFlow id="f5" sourceRef="n2" targetRef="n1"/>
<sequenceFlow id="f6" sourceRef="n3" targetRef="n5"/>
<sequenceFlow id="f7" sourceRef="n4" targetRef="n1"/>
<sequenceFlow id="f8" sourceRef="n5" targetRef="n4"/>
</process>
</definitions>)BPMN",
{"ent_e0", "ent_n0", "ent_n3", "ent_n5", "f4", "f5", "f7", "sink", "source"},
{"U|~|f4,f5,f7|ent_e0,ent_n0,f4", "e0|~|ent_e0|sink", "n0|A0|ent_n0|ent_e0", "n2|A3|source|f5", "n3|A2|ent_n3|ent_n5", "n5|A1|ent_n5|f7"},
{"source"}, {"sink"}},
