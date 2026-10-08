import { type LlmImage } from "./llm";

export const LLM_IMAGE_ACCEPT="image/png,image/jpeg,image/webp,image/gif";
export async function readLlmImage(file:File):Promise<LlmImage> {
  if(file.size===0||file.size>1572864)throw new Error("Each image must be nonempty and at most 1.5 MiB. The complete request must fit 2 MiB.");
  const bytes=new Uint8Array(await file.arrayBuffer());
  const ascii=(offset:number,count:number)=>String.fromCharCode(...bytes.slice(offset,offset+count));
  let media_type:string;
  if(bytes[0]===0x89&&ascii(1,7)==="PNG\r\n\x1a\n")media_type="image/png";
  else if(bytes[0]===0xff&&bytes[1]===0xd8&&bytes[2]===0xff)media_type="image/jpeg";
  else if(["GIF87a","GIF89a"].includes(ascii(0,6)))media_type="image/gif";
  else if(ascii(0,4)==="RIFF"&&ascii(8,4)==="WEBP")media_type="image/webp";
  else throw new Error("Choose a PNG, JPEG, WebP or GIF image.");
  let binary="";for(let i=0;i<bytes.length;i+=8192)binary+=String.fromCharCode(...bytes.subarray(i,i+8192));
  return {name:file.name,media_type,data:btoa(binary)};
}
export function llmImageUrl(image:LlmImage):string {return "data:"+image.media_type+";base64,"+image.data;}
