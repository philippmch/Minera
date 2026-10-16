/** Encode native strings without File/Blob's USVString replacement of lone UTF-16 surrogates. */
export function nativeStringBytes(text: string): Uint8Array<ArrayBuffer> {
  const bytes = new Uint8Array(text.length * 3); let position = 0;
  for (let i = 0; i < text.length; ++i) {
    let code = text.charCodeAt(i);
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < text.length) {
      const low = text.charCodeAt(i + 1);
      if (low >= 0xdc00 && low <= 0xdfff) {code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00; ++i;}
    }
    if (code < 0x80) bytes[position++] = code;
    else if (code < 0x800) {bytes[position++] = 0xc0 | code >> 6; bytes[position++] = 0x80 | code & 0x3f;}
    else if (code < 0x10000) {bytes[position++] = 0xe0 | code >> 12; bytes[position++] = 0x80 | code >> 6 & 0x3f; bytes[position++] = 0x80 | code & 0x3f;}
    else {bytes[position++] = 0xf0 | code >> 18; bytes[position++] = 0x80 | code >> 12 & 0x3f; bytes[position++] = 0x80 | code >> 6 & 0x3f; bytes[position++] = 0x80 | code & 0x3f;}
  }
  return bytes.subarray(0, position);
}

/** Multipart strings normalize CR/LF and replace lone surrogates; byte fields retain both. */
export function appendNativeMultipartField(body: FormData, key: string, raw: string): void {let lone = false; for (let index = 0; index < raw.length; ++index) {const code = raw.charCodeAt(index); if (code >= 0xd800 && code <= 0xdbff) {const next = raw.charCodeAt(index + 1); if (next >= 0xdc00 && next <= 0xdfff) ++index; else lone = true;} else if (code >= 0xdc00 && code <= 0xdfff) lone = true;} if (lone || /[\r\n]/.test(raw)) body.append(key, new Blob([nativeStringBytes(raw)], {type: "application/octet-stream"}), `${key}.txt`); else body.append(key, raw);}
