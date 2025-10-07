# cTOML — A Lightweight TOML Parser & Writer in C

cTOML is a lightweight, extensible, and C11‑compatible library for reading and writing TOML configuration files.

It focuses on simplicity, speed, and embedded compatibility — with no external dependencies.

## Feature set

| Feature                    | Description                                           |
| -------------------------- | ----------------------------------------------------- |
| ✅ Read & Write TOML 1.0   | Supports integers, floats, booleans, and strings      |
| ✅ Multiline strings       | Reads and writes triple‑quoted """ ... """ blocks     |
| ✅ Arrays                  | Handles typed and mixed arrays [1, 2, 3], ["a", "b"]  |
| ✅ Nested tables           | Parses [server.config] and dotted keys like a.b.c = 3 |
| ✅ Hierarchical data model | TomlDoc → TomlTable → TomlEntry structure             |
| ✅ Type‑safe getters       | toml_get_int, toml_get_bool, toml_get_string, etc.    |
| ✅ Structured errors       | Collects parse errors with line numbers               |

## 🧑‍💻 License

[MIT License](LICENSE)

(c) 2025 Deondre English.

You’re free to use, modify, and distribute — no attribution required, but appreciated.
