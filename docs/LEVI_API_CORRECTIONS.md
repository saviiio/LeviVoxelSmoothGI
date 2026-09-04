# Correções aplicadas a partir do resumo da API LeviLaunchroid

Este arquivo mapeia as conclusões do TXT de resumo para mudanças concretas na beta.8.

## Hook API

- Detours possuem typedefs com as assinaturas GLES/EGL correspondentes.
- O original vem do `HookHandle`; o detour não chama o endereço target cru.
- Todos os `HookHandle` são membros persistentes de `GlHooks`.
- Prioridade: `HookPriority::Normal`.
- Instalação ocorre somente depois que `libminecraftpe.so` foi observado como módulo carregado.
- Cada resolução exige endereço não-zero, `installed()` e original não-nulo.
- Falha em um hook obrigatório desfaz o conjunto parcial.

## Mod API

- Instância long-lived e registro único por `PL_REGISTER_MOD`.
- `NativeMod::current()` usado na construção da instância.
- `load/enable/disable/unload` separados por responsabilidade.
- `getDataDir`, `getConfigDir` e `getResourceDir` substituem caminhos improvisados.
- Exceções são convertidas em log + `false`.

## Config API

- Configuração migrou para `pl::config::ConfigFile<lvsgi::Config>`.
- `Config` é aggregate público e possui `int version = 8`.
- `Schema<Config>` fornece títulos e limites.
- Como schema não é enforcement runtime, `sanitizeConfig()` faz clamps equivalentes e salva a versão normalizada.

## Input API

Nenhum callback de input é necessário para o funcionamento da beta.8. Isso evita registrar callbacks sem API de unregister. Se um toggle futuro for adicionado, ele deverá ser registrado uma vez e gated por estado `enabled`.

## Mod Menu API

Não foi adicionado Mod Menu à beta.8. O objetivo principal é provar primeiro o caminho RenderDragon sem introduzir mais uma superfície ABI. O HUD diagnóstico pode ser adicionado depois usando a API oficial, vinculado ao `getSelf().getId()`.

## Memory Hook / Signature

- O caminho principal agora usa `resolveSignature(..., "libminecraftpe.so")`.
- O perfil é preso ao Build ID enviado.
- As assinaturas são stubs PLT ARM64 de funções com protótipos públicos conhecidos, o que evita adivinhar a ABI de uma função C++ interna não simbolizada.
- O utilitário `validate_direct_hook_profile.py` exige uma ocorrência exata por assinatura.

## Ordem de prova em runtime

A implementação registra explicitamente:

`MOD ENABLED -> MINECRAFT MODULE FOUND -> BUILD ID MATCHED -> SIGNATURE FOUND -> HOOK INSTALLED -> HOOK CALLED -> PATCH APPLIED -> VOXEL RUNTIME READY -> GI DISPATCH`

A matemática da GI só deve ser investigada como causa principal depois que essa cadeia estiver ativa.
