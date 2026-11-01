# Standalone arcade runtime

Clayfight runs independently of Nile Archive: native C combat, SDL desktop,
UIKit iOS, indexed sprite/photo support and a standalone CAGE decoder.
The only retail input is the supported Primal Rage 2.3 December 1994 arcade
chip set. DOS resources, executable loaders, conversion and AI/oracle tooling
are removed. Earlier implementation records remain in Git history.

See [MIGRATION.md](MIGRATION.md) for the extraction record, the root README for
build/test commands, [primal-rage-combat.md](primal-rage-combat.md) for game
mechanics and limits, and [primal-rage-arcade.md](primal-rage-arcade.md) for the
runtime asset/state contract.
