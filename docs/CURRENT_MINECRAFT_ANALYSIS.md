# Perfil dos arquivos Minecraft/RenderDragon mais recentes enviados

## libminecraftpe.so

Build ID ELF observado: `868e275cb295e9a275bb29d2258edc2f7dc48761`.

O perfil gerado em `generated/CurrentMinecraftProfile.hpp` é baseado nas fontes ESSL 3.10 extraídas dos `.material.bin` enviados, não em nomes inventados.

## DeferredShading

O shader atual expõe os sinais necessários ao caminho novo, incluindo:

- `s_ColorMetalnessSubsurface`
- `s_EmissiveAmbientLinearRoughness`
- `s_SceneDepth`
- `s_Normal`
- `u_invProj`
- `u_invView`
- `SubPixelOffset`
- `v_projPosition`
- `v_texcoord0`

A reconstrução usada pelo mod segue o espaço do shader atual: profundidade é transformada para [-1,1], `SubPixelOffset` é aplicado ao XY de projeção, depois usa `u_invProj` e `u_invView`.

A normal usa a codificação octaédrica presente no G-buffer atual.

## SSBOs

As variantes Deferred analisadas já utilizam bindings altos (10 a 13). O patcher examina todos os `layout(binding=...)` da fonte real e escolhe dinamicamente um binding SSBO livre em vez de fixar um número que poderia colidir.

## Reflexos Android atuais

`render/platform_config/android/reflection_configuration.android.json` deixa `ssr_enabled` falso em low, medium e high e verdadeiro em ultra. Por isso o H-DDA novo não depende do passe SSR ser criado: ele é composto no DeferredShading. O shader `ScreenSpaceReflections` é neutralizado somente quando a configuração `disableNativeSsr` está ligada.

## Item na mão

Foram analisadas as nove famílias atuais:

- ItemInHandColor
- ItemInHandColorGlint
- ItemInHandTextured
- ItemInHandPrepass
- ItemInHandPrepassGlint
- ItemInHandPrepassTextured
- ItemInHandForwardPBR
- ItemInHandForwardPBRGlint
- ItemInHandForwardPBRTextured

Os binários armazenam estágios em pares Vertex/Fragment. O conjunto atual possui 280 pares brutos; depois de deduplicar fontes e pares, a base de prewarm contém 138 shaders únicos e 220 programas únicos.

## beta.8: caminho direto dentro do libminecraftpe.so

A análise ELF da biblioteca atual mostrou imports diretos de EGL/GLES e stubs PLT próprios para o backend gráfico. Em vez de depender de um `libGLESv2.so` externo ou de observar o loader Android, a beta.8 resolve os stubs dentro de `libminecraftpe.so` com a Signature API do preloader.

O perfil contém assinaturas únicas para 15 entry points obrigatórios. O validador offline encontrou exatamente uma ocorrência de cada padrão na biblioteca com Build ID `868e275cb295e9a275bb29d2258edc2f7dc48761`.

A análise de chamadas ARM64 encontrou uma chamada direta a `glShaderSource@plt` e a `glCompileShader@plt` no caminho de compilação BGFX desta build. Isso torna o stub PLT um alvo especialmente útil: a assinatura C da função é conhecida, enquanto um hook em uma função C++ interna não simbolizada exigiria adivinhar ABI/tipos.

As variantes `RenderChunkForwardPBR` analisadas usam SSBOs 10 a 14 e não usam binding 15; as variantes Deferred usam 10 a 13. Em um dispositivo com 16 bindings, portanto, o seletor dinâmico escolhe 15 para os shaders atuais sem colidir com os bindings observados nesses materiais. O patcher continua verificando a fonte real antes de escolher o binding.
