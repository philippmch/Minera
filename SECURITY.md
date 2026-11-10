# Security

## Reporting a vulnerability

Please report security problems privately through GitHub: open the
repository's **Security** tab and choose **Report a vulnerability**. Do not
open a public issue for them.

Include what you found, how to reproduce it and which commit you tested. You
should get a first answer within a week.

## Scope

`minera_server` is meant for one person on their own machine. It listens on
`127.0.0.1` only, answers browsers on its own origin and the Vite dev and
preview ports (more with `--allow-origin`), and caps upload sizes and memory
use (see the README). Reports that matter most:

- reading or writing files outside what a request names;
- crashes, hangs or memory exhaustion from a crafted log or model file;
- a web page in another origin driving the local server;
- leaking a language-model API key or other credentials.

Exposing the server to a network yourself, for example through a reverse
proxy, is outside what it is designed for.
