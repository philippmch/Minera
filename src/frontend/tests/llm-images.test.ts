import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { PromptChat } from "../src/components/AskModel";
import { readLlmImage,llmImageUrl } from "../src/llm-images";
import { llmCompletionRequest,llmExtraPayload,requestLlmCompletion } from "../src/llm";

afterEach(()=>vi.unstubAllGlobals());
describe("model images and provider payloads",()=>{
  it("detects bytes independently of a spoofed file MIME type and preserves image data",async()=>{
    const bytes=new Uint8Array([137,80,78,71,13,10,26,10]);
    const image=await readLlmImage(new File([bytes],"diagram α.png",{type:"text/plain"}));
    expect(image).toEqual({name:"diagram α.png",media_type:"image/png",data:"iVBORw0KGgo="});
    expect(llmImageUrl(image)).toBe("data:image/png;base64,iVBORw0KGgo=");
  });
  it("rejects missing oversized and invalid images",async()=>{
    await expect(readLlmImage(new File([],"empty.png"))).rejects.toThrow(/nonempty/);
    await expect(readLlmImage(new File([new Uint8Array(1572865)],"big.png"))).rejects.toThrow(/1.5 MiB/);
    await expect(readLlmImage(new File(["plain"],"wrong.png",{type:"image/png"}))).rejects.toThrow(/PNG/);
  });
  it("retains arbitrary nested fields and overrides while rejecting invalid payload drafts",()=>{
    expect(llmExtraPayload('{"model":"override","metadata":{"name":"α"},"generationConfig":{"topP":0.8}}')).toMatchObject({model:"override",metadata:{name:"α"}});
    for(const value of ("[] null 123".split(" ")))expect(()=>llmExtraPayload(value)).toThrow(/object/);
    expect(()=>llmExtraPayload("{")).toThrow(/valid JSON/);
    expect(()=>llmExtraPayload('{"stream":true}')).toThrow(/separate/);
  });
  it("uses provider temperature ranges and carries complete attachment controls",()=>{
    const image={name:"diagram.png",media_type:"image/png",data:"iVBORw0KGgo="};
    const input=llmCompletionRequest("openai","fixture","Explain α","256","2",{openai_api:"responses",images:[image],extra_payload:{top_p:0.8}});
    expect(input).toMatchObject({temperature:2,openai_api:"responses",images:[image],extra_payload:{top_p:0.8}});
    expect(()=>llmCompletionRequest("anthropic","fixture","Explain","256","1.1")).toThrow(/Temperature/);
    expect(()=>llmCompletionRequest("openai","fixture","Explain","256","",{images:Array(9).fill(image)})).toThrow(/eight/);
  });
  it("sends the submitted prompt images API choice and extras in one bounded JSON body",async()=>{
    const fetch=vi.fn(async()=>({ok:true,json:async()=>({family:"LLM_COMPLETION",provider:"openai",model:"fixture",text:"Answer",finish_reason:"completed",usage:{input_tokens:2,output_tokens:1},response:{output:[]}})}));vi.stubGlobal("fetch",fetch);
    const input=llmCompletionRequest("openai","fixture","Explain α","256","",{openai_api:"responses",images:[{name:"diagram.png",media_type:"image/png",data:"iVBORw0KGgo="}],extra_payload:{reasoning:{effort:"low"}}});
    await requestLlmCompletion(input);expect(JSON.parse((fetch.mock.calls[0] as unknown as [string,RequestInit])[1].body as string)).toEqual(input);
    await expect(requestLlmCompletion({...input,prompt:"a".repeat(2097153)})).rejects.toThrow(/bound/);expect(fetch).toHaveBeenCalledTimes(1);
  });
  it("exposes attachment selection Responses selection and provider payload review",()=>{
    const markup=renderToStaticMarkup(createElement(PromptChat,{mining:false}));
    expect(markup).toContain("Image attachments");expect(markup).toContain("multiple");expect(markup).toContain("Responses");expect(markup).toContain("Extra provider payload");expect(markup).toContain("override generated provider fields");
  });
});
