# Engineering diagrams

Six editable Mermaid sources are maintained here. In these flowcharts, thick arrows (`==>`) are power, thin arrows are data/control, and undirected lines are reference/mechanical relationships. Labels distinguish high-current propulsion from regulated rails. BEC output is explicitly unused. All GPIOs are provisional pending the actual DevKit pinout and ESC identification.

[System](system-block-diagram.mmd), [glove](glove-wiring.mmd), [drone](drone-wiring.mmd), [motor layout](motor-layout.mmd), [firmware](firmware-architecture.mmd), [power](power-distribution.mmd).

The [motor-layout SVG](motor-layout.svg) is a standalone top-view drawing; the Mermaid motor layout expresses connectivity rather than geometric placement. Rotation is viewed from above. Code coefficients and the CSV mapping use the same M1–M4 convention.

GitHub renders Mermaid inside Markdown fenced blocks; raw .mmd files may appear as source. The root README embeds the system, motor and power diagrams. For local SVG rendering install Mermaid CLI from npm and run:
```sh
npm install --no-save @mermaid-js/mermaid-cli@11.4.2
npx mmdc -i diagrams/system-block-diagram.mmd -o build/system.svg
```
Repeat for each source. Renderer execution must be recorded separately; syntax/static consistency checks are not visual verification.
