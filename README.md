# CasePath 0.2.5

CasePath is a Qt 6 Widgets application for managing the Lester McCabe VA/NOK and California adoption-record workflow.

## 0.2.5 changes

- Removed Jeremiah's personal Monday/Tuesday availability from CasePath and from the Mistral handoff. Task Orchestrator owns user availability.
- CasePath retains external constraints such as the San Diego Superior Court Adoption Office hours (Monday-Friday, 8:30 AM-4:00 PM).
- Replaced ambiguous `Dudley/Doug` UI text with the canonical CasePath label `Dudley O'Neal (known as Doug)`.
- Preserved the `<` / `>` tab navigation, T14 context behavior, certificate diagnostics, Pi/Mistral workflow, and `/tmp/mistralinteraction.sh` interaction launcher.

## Shared CasePath time helper

CasePath does not duplicate the CasePath time-code mapping. Encoding and decoding are delegated to:

`/opt/casepath_time/casepath_time.py`

through `QProcess`.

## Task Orchestrator

CasePath remains an interactive daily administrative task and registers non-fatally when Task Orchestrator is available:

`taskorchestrator register --id casepath --name "CasePath" --exec /home/we6jbo/.local/bin/casepath --priority 75 --duration 45 --cadence daily`

Task Orchestrator is optional and is not a program dependency. CasePath does not duplicate the user's availability calendar.

## Mistral interaction

Tab 10 writes `/tmp/mistralinteraction.sh`, copies `/tmp/mistralinteraction.sh` to the clipboard, and instructs the user to open a normal terminal, paste the command, and press Enter.
