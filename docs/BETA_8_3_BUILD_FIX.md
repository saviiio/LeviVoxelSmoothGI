# beta.8.3 build fix

GitHub Actions with Android NDK 28.2 clang failed because `pl::config::FieldSchema`
was returned with partial C++20 designated initializers while the mod target builds with
`-Werror`. Android clang emits `-Wmissing-designated-field-initializers` for the omitted
members (`title`, `description`, `minimum`, `maximum`, `readOnly`).

The schema now default-constructs `FieldSchema` and assigns only the required fields via
`constexpr` helper functions. This preserves the exact schema metadata without suppressing
compiler diagnostics. The local Android syntax check now explicitly enables
`-Wmissing-designated-field-initializers` so this class of regression is checked before CI.
