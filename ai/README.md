# ai/ — AI Assisted mode

Components used only when ZDoc runs with `--mode ai`.

- [`bob_client`](bob_client/) — invokes the Bob CLI per extracted symbol to generate a
  brief Mermaid block diagram, and parses the response back into the documentation model.

In offline mode (the default) nothing under `ai/` is invoked.

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification.
