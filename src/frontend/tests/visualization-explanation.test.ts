import { describe,expect,it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { visualizationPrompt,visualizationSize,type VisualizationDraft } from "../src/visualization-explanation";
import { VisualizationExplain } from "../src/components/VisualizationExplain";
import { PromptChat } from "../src/components/AskModel";

describe("automatic visualization explanations",()=>{
 it("preserves selected visualization and dataset in a reviewed prompt",()=>{const prompt=visualizationPrompt("Petri net α","original.xes");expect(prompt).toContain("Petri net α");expect(prompt).toContain("original.xes");expect(prompt).toContain("visible patterns");});
 it("fits complete large panels without cropping or changing aspect ratio",()=>{expect(visualizationSize(800,600)).toEqual({width:800,height:600,scale:1});const size=visualizationSize(3200,1600);expect(size).toEqual({width:1600,height:800,scale:0.5});expect(()=>visualizationSize(0,10)).toThrow(/visible/);expect(()=>visualizationSize(Infinity,10)).toThrow(/visible/);});
 it("offers diagram chart and table selection with capture before model execution",()=>{const markup=renderToStaticMarkup(createElement(VisualizationExplain,{dataset:"original.xes",busy:false,onUse:()=>{}}));expect(markup).toContain("Visualization to explain");expect(markup).toContain("diagram, chart or table");expect(markup).toContain("Prepare visualization explanation");});
 it("opens the captured image and exact prompt in the existing provider workflow",()=>{const draft:VisualizationDraft={id:"owned",title:"OCDFG",dataset:"original.json",image:{name:"graph.png",media_type:"image/png",data:"iVBORw0KGgo="},prompt:"Explain OCDFG α.",width:800,height:600};const markup=renderToStaticMarkup(createElement(PromptChat,{mining:false,visualization:draft}));expect(markup).toContain("Explain OCDFG α.");expect(markup).toContain("800");expect(markup).toContain("graph.png");expect(markup).toContain("data:image/png;base64,iVBORw0KGgo=");expect(markup).toContain("Send displayed prompt");});
});
