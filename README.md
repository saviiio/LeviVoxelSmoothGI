# LeviVoxelSmoothGI 1.0.0 beta.8.7

## Compatibilidade dinâmica da beta.8.7

A beta.8.7 remove o Build ID do Minecraft como requisito de execução. O mod agora descobre os hooks GLES/EGL diretamente no `libminecraftpe.so` carregado: lê `DT_JMPREL`, `DT_SYMTAB` e `DT_STRTAB`, encontra o slot GOT de cada símbolo importado e localiza o stub PLT AArch64 correspondente decodificando `ADRP/LDR/ADD/BR`. O Build ID observado é usado apenas para diagnóstico e para salgar o cache de programas.

Os offsets e assinaturas da build conhecida continuam no pacote somente como material de validação/regressão; não são usados para decidir se uma versão do Minecraft pode executar o mod. Os patches principais de Deferred e RenderChunk já são reconhecidos por assinatura estrutural estrita. Hashes exatos continuam apenas como confirmação de perfis conhecidos e para a substituição opcional/destrutiva de SSR, que não é necessária para ativar a GI.

Antes de instalar qualquer hook, a beta.8.7 resolve o conjunto inteiro de 15 símbolos. Cada stub é relido com `pl::memory::readBytes()` e validado como PLT AArch64 imediatamente antes do `HookHandle`, preservando a regra de não deixar um conjunto parcial de hooks ativo.

## Correção de GI da beta.8.5

A beta.8.5 aplica a releitura 5x das páginas oficiais `types-and-macros`, `signature`, `patch` e `mod` e corrige dois caminhos que podiam deixar a GI silenciosamente inativa mesmo com o mod carregado:

- o Deferred/RenderChunkForward não depende mais apenas do hash do texto completo recebido por `glShaderSource`; uma variação de preâmbulo/defines do BGFX agora é aceita quando o fingerprint estrutural estrito da família atual confere;
- câmera e serial de frame agora são publicados pelo próprio SSBO. O serial do próximo frame é escrito pelo runtime no header 15 e lido pelos fragment shaders, portanto não depende de `glUseProgram` ocorrer novamente a cada frame.

Também foram adicionados diagnósticos da cadeia inteira (`linked`, `used`, `camCommit`, `capture`, `source`, `computeSource`, `computeNonzero`, `computeFrame`, `giSample`) e a validação dos bytes dos offsets PLT passou a usar `pl::memory::readBytes` antes de criar os `HookHandle`.

Os detalhes e os dois TXT da releitura estão em `docs/BETA_8_5_GI_RUNTIME_FIX.md`, `docs/LEVI_API_4_PAGES_READ_5X.txt` e `docs/LEVI_API_4_PAGES_SUMMARY.txt`.

## Correção de runtime da beta.8.4

A beta.8.4 corrige o crash nativo observado em `Rendering Pool` durante a compilação de shaders:

- não usa mais `resolveSignature()` 15 vezes enquanto o RenderDragon já está criando workers; os offsets PLT do Build ID validado são usados diretamente e os 16 bytes de cada stub ainda são verificados antes do hook;
- hooks parcialmente instalados ficam em modo pass-through até o conjunto completo estar ativo;
- `glCompileShader` nunca é adiado para outro contexto e `GL_COMPILE_STATUS` nunca é falsificado;
- patches invasivos de Deferred/SSR exigem hash exato do perfil atual;
- se um shader patchado falhar, o mod registra o log do compilador, restaura o source original e recompila no mesmo contexto.

Mod nativo ARM64 para LeviLauncher/LeviLaunchroid no Android. Esta revisão aplica as conclusões da releitura da API oficial do LeviLauncher: o caminho gráfico principal não depende mais de observar `dlopen` ou de acertar o namespace de `libEGL.so`/`libGLESv2.so`.

## Histórico: mudança principal da beta.8 (substituída pelo resolver dinâmico da beta.8.7)

A build exata de `libminecraftpe.so` enviada foi analisada diretamente. O ELF tem Build ID:

`868e275cb295e9a275bb29d2258edc2f7dc48761`

Ela importa diretamente os entry points GLES/EGL usados pelo backend BGFX. Para esta build foram extraídas assinaturas ARM64 únicas dos stubs PLT de `glShaderSource`, `glCompileShader`, `glLinkProgram`, `glUseProgram`, `eglGetProcAddress`, `eglSwapBuffers` e das demais funções necessárias.

O fluxo agora é:

```text
LeviLauncher lifecycle
  -> aguarda libminecraftpe.so real aparecer
  -> lê o GNU Build ID da imagem ELF já mapeada
  -> exige o Build ID exato do perfil
  -> usa offsets PLT pré-validados do perfil exato
  -> pl::memory::readBytes confirma os bytes esperados no endereço
  -> HookHandle persistente, prioridade Normal
  -> valida installed() e ponteiro original
  -> intercepta os stubs PLT dentro do próprio Minecraft
  -> eglGetProcAddress@plt também cobre funções pedidas dinamicamente pelo BGFX
```

Isso elimina a principal fragilidade das betas 6/7: não é mais necessário abrir EGL/GLES artificialmente nem esperar que o RenderDragon resolva novamente um ponteiro depois do hook.

`tools/validate_direct_hook_profile.py` verifica o Build ID e exige que cada assinatura direta ocorra exatamente uma vez no `libminecraftpe.so` de referência.

## Lifecycle e configuração

A implementação segue a API pública do `preloader-android 0.2.2`:

- instância C++ persistente registrada uma única vez com `PL_REGISTER_MOD`;
- `ll::mod::NativeMod::current()` fornece o objeto do mod;
- `load()` cria os diretórios oficiais e carrega `pl::config::ConfigFile<Config>`;
- `enable()` arma os hooks;
- `disable()` para workers e faz `reset()` dos `HookHandle`;
- `unload()` libera o estado C++ restante;
- exceções são contidas dentro das fronteiras de lifecycle;
- cache persistente fica em `getDataDir()`;
- configuração fica em `getConfigDir()`;
- a base de prewarm fica em `getResourceDir()`.

O schema JSON possui limites para dimensões, GI, cache e agendamento. Como o schema é metadado e não aplica os limites sozinho, `sanitizeConfig()` também faz clamp em runtime e persiste os valores normalizados.

## Telemetria para provar que o mod executa

A beta.8.7 não considera “mod instalado” equivalente a “renderer interceptado”. O log registra a cadeia separadamente:

```text
MOD ENABLED
MINECRAFT MODULE FOUND
BUILD ID OBSERVED (diagnostic only)
DYNAMIC PLT RESOLVED: ...
HOOK INSTALLED: ...
GRAPHICS ACTIVE
HOOK CALLED -> SHADER/RENDER PATH INTERCEPTED
PATCH APPLIED
VOXEL RUNTIME READY
HEARTBEAT ... patched=... linked=... used=... source=... computeNonzero=... giSample=...
```

O heartbeat inclui:

- módulo encontrado;
- Build ID observado (somente diagnóstico/cache);
- resolver ELF/PLT dinâmico pronto;
- conjunto de hooks ativo;
- total de chamadas dos detours;
- frames interceptados por `eglSwapBuffers`;
- chamadas de `glShaderSource`;
- shaders transformados;
- estado do volume voxel;
- dispatches de GI;
- cache hits/misses.

Assim, se a imagem continuar sem GI, é possível saber exatamente se a falha está no hook, classificador, criação do SSBO ou execução do compute.

## Redução de stutter de shaders

- intercepta criação/fonte/compilação/link/uso de shaders e programas no caminho do Minecraft;
- cache persistente de `glProgramBinary` por fonte, estágio, atributos, transform feedback, Build ID e fingerprint do driver;
- leitura de cache ocorre em worker de I/O, não na render thread;
- em cache hit a compilação GLSL pode ser evitada;
- captura de novos binários é atrasada e limitada a poucos eventos após frames;
- usa `GL_KHR_parallel_shader_compile` quando disponível;
- base atual de ItemInHand: 138 shaders únicos e 220 programas deduplicados para prewarm progressivo.

Um shader realmente novo, ausente tanto do cache quanto do prewarm, ainda pode exigir compilação do driver. GLES não fornece mecanismo seguro para garantir custo zero nesse primeiro caso.

## Reflexos H-DDA

- voxel base = exatamente 1 bloco;
- hierarquia 2, 4 e 8 blocos somente para acelerar espaços vazios;
- hit final sempre acontece no voxel de 1 bloco;
- seis faces possuem radiância separada;
- H-DDA é composto no DeferredShading, portanto não depende do SSR Android existir em low/medium/high.

## GI floodfill direcional

A implementação segue a topologia observada no pacote Java de referência:

- uma superfície injeta a fonte no voxel de ar imediatamente externo à sua face;
- os seis lados permanecem independentes;
- propagação usa somente vizinhos axiais;
- caminho reto domina;
- `giTurn` controla transferência para faces laterais;
- `giBackTurn` controla retorno;
- `giDecay` controla perda;
- receptor não usa taps diagonais que atravessem cantos.

Foi acrescentada uma barreira `GL_SHADER_STORAGE_BARRIER_BIT` **antes** do dispatch compute para tornar visíveis ao floodfill as escritas SSBO feitas pelos fragment shaders do frame anterior. Há outra barreira após o compute para tornar a GI visível aos draws seguintes.

## Correção da linha de pixels da tela

O fallback antigo com `glBlitFramebuffer` no framebuffer default permanece no código, mas `fixScreenEdgeRow` agora é `false` por padrão. Em GPUs tile-based, especialmente Mali, esse blit pode forçar um resolve completo e contribuir para ANR. A correção definitiva deve ser aplicada no passe final de apresentação do RenderDragon, não por uma cópia do framebuffer default.

## Perfil atual

Arquivos usados para gerar o perfil:

- `libminecraftpe.so` atual enviado;
- `DeferredShading.material.bin`;
- `RenderChunkForwardPBR.material.bin`;
- `ScreenSpaceReflections.material.bin`;
- nove famílias `ItemInHand*`.

`generated/CurrentMinecraftProfile.hpp` contém os hashes ESSL e as assinaturas PLT específicas dessa build. O mod recusa silenciosamente aplicar hooks version-specific em outro Build ID; ele registra o mismatch no log em vez de arriscar um endereço incorreto.

## Compilação Android

O target Android usa C++20 porque `preloader-android 0.2.2` usa Concepts/`requires`. O núcleo portátil continua C++17.

GitHub Actions usa:

- NDK `28.2.13676358`;
- `arm64-v8a`;
- Android API 28;
- `c++_shared`;
- `preloader-android 0.2.2`;
- cache/reuso de `preloader`, `fmt` e `FetchContent` antes de baixar novamente.

Para build direto em Termux, se houver NDK host AArch64 compatível:

```bash
bash scripts/build-termux.sh
```

Se o clang do NDK instalado for x86_64 e der `Exec format error`, use o workflow `.github/workflows/build-android.yml`.

## Estrutura do .levipack

```text
levi_voxel_smooth_gi/
  manifest.json
  liblevi_voxel_smooth_gi.so
  config/
    config.json
    config.schema.json
  resources/
    current_item_programs.lvpr
```

## Validação executada

- núcleo host: GCC 14.2, C++17, `-Wall -Wextra -Wpedantic -Werror`;
- `ctest`: aprovado;
- fontes Android: `clang++ -std=c++20 -fsyntax-only -Werror` com stubs de API para detectar erros C++ antes do NDK;
- perfil direto: Build ID exato e 15/15 assinaturas PLT com exatamente uma ocorrência no `libminecraftpe.so` fornecido;
- configuração: teste de clamp de valores fora dos limites.

Este ambiente não executa Minecraft Bedrock/RenderDragon nem o driver GLES do aparelho. Portanto a instalação e execução real do `.so` ainda precisam ser verificadas no Android; agora os logs distinguem cada estágio para que um erro não seja confundido com “GI fraca”.