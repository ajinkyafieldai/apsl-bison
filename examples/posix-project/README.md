# POSIX Bison VS Code project

Open this directory as the VS Code workspace.

Press **F5** and choose **Bison: Run recipe on POSIX**.

VS Code will:

1. configure and build the host Bison tools,
2. start a POSIX Bison server on `127.0.0.1:8080`,
3. upload `recipe.lua`,
4. execute it with the APSL Lua runtime,
5. stream `print()` output back to the terminal.

Lua failures use compiler-style diagnostics:

```text
recipe.lua:2: error: attempt to index a nil value
```

The same diagnostic format is understood by the **Bison: Run recipe** task problem matcher, so errors appear in VS Code's Problems view and navigate to the source line.
