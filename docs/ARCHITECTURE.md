# Arquitetura beta.8

## 1. Lifecycle oficial

`Mod.cpp` mantém uma única instância C++ registrada por `PL_REGISTER_MOD` e conserva a referência retornada por `ll::mod::NativeMod::current()`.

- `load()`: cria `getConfigDir()`/`getDataDir()`, carrega `ConfigFile`, sanitiza limites e constrói o runtime CPU.
- `enable()`: arma cache/prewarm e inicia a espera por `libminecraftpe.so`.
- `disable()`: sinaliza workers, espera a saída deles e reseta todos os `HookHandle`.
- `unload()`: repete cleanup de forma idempotente e libera os objetos C++.

Nenhuma exceção é deixada atravessar uma função de lifecycle.

## 2. Bootstrap sem hook de loader

A beta.8 não hooka `dlopen` nem `android_dlopen_ext`.

Um worker leve usa `dl_iterate_phdr()` apenas para observar quando a imagem real `libminecraftpe.so` já está mapeada. Ele também lê o `PT_NOTE` GNU Build ID diretamente da imagem ELF em memória. Somente se o Build ID for exatamente o perfil atual a instalação continua.

Esse worker não toca em GLES e não executa I/O de shader.

## 3. Hooks diretos no próprio libminecraftpe.so

O `libminecraftpe.so` atual possui stubs PLT para as funções GLES/EGL necessárias. As sequências ARM64 desses stubs foram extraídas da build enviada e verificadas como únicas.

Para cada alvo:

1. `resolveSignature(signature, "libminecraftpe.so")`;
2. endereço precisa ser não-zero;
3. cria `HookHandle` persistente com `HookPriority::Normal`;
4. `installed()` precisa ser verdadeiro;
5. o ponteiro `original` precisa ser não-nulo.

Se qualquer hook obrigatório falhar, todo o conjunto parcial é removido. Não fica uma instalação metade ativa.

Alvos diretos:

- `eglGetProcAddress@plt`;
- `eglSwapBuffers@plt`;
- `glCreateShader@plt`;
- `glShaderSource@plt`;
- `glCompileShader@plt`;
- `glGetShaderiv@plt`;
- `glGetShaderInfoLog@plt`;
- `glDeleteShader@plt`;
- `glCreateProgram@plt`;
- `glAttachShader@plt`;
- `glDetachShader@plt`;
- `glLinkProgram@plt`;
- `glGetProgramiv@plt`;
- `glDeleteProgram@plt`;
- `glUseProgram@plt`.

O hook de `eglGetProcAddress@plt` também devolve nossos detours quando BGFX pede dinamicamente esses entry points. Portanto existem dois caminhos complementares: chamadas diretas do Minecraft e resolução dinâmica.

## 4. Cache anti-stutter

A chave SHA-256 inclui:

- Build ID do Minecraft;
- vendor/renderer/versão GLES/GLSL;
- tipo e fonte integral de cada shader;
- bindings de atributos observados;
- varyings e mode de transform feedback;
- versão interna da chave.

`glCompileShader` pode ser adiado quando program binary é suportado. `glLinkProgram` consulta primeiro o cache em memória. Hit: `glProgramBinary`. Miss: compila os shaders ainda adiados e linka normalmente.

O worker do cache faz I/O de disco. A render thread nunca procura arquivos no filesystem. Captura de binário ocorre com atraso e intervalo configuráveis.

## 5. Prewarm ItemInHand

`resources/current_item_programs.lvpr` contém as combinações deduplicadas atuais. O arquivo é lido por worker. Depois, poucos programas são lançados por frame. Com `KHR_parallel_shader_compile`, programas podem permanecer pendentes até `GL_COMPLETION_STATUS_KHR` informar conclusão.

## 6. SSBO voxel

Voxel base = 1 bloco. Ring addressing usa coordenada mundial e tag hash para distinguir células físicas reutilizadas.

Dados base por célula:

- tag;
- máscara das seis faces sólidas;
- seis cores de face RGB565;
- seis fontes GI RGB565;
- seis canais GI ping;
- seis canais GI pong.

Dois RGB565 são compactados em cada `uint`.

A hierarquia 2/4/8 armazena tag + ocupação.

## 7. Sincronização GPU

Fragment shaders escrevem ocupação, faces e fontes no SSBO. Antes do compute floodfill há:

`glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)`

Isso garante que as escritas shader-storage anteriores estejam visíveis ao compute. Após `glDispatchCompute` há outra barreira do mesmo tipo para que a GI calculada fique disponível aos draws posteriores.

A alocação do volume não acontece durante `glShaderSource`/`glCompileShader`; ela é atrasada para um frame real observado em `eglSwapBuffers` e possui retry.

## 8. H-DDA

Níveis: 8 -> 4 -> 2 -> 1 bloco. Níveis grossos apenas pulam vazio. O hit e a cor sempre vêm da célula base de 1 bloco e de sua face correspondente.

## 9. Floodfill direcional

Cada voxel de ar mantém seis canais. Fonte de uma face sólida é injetada na célula de ar imediatamente externa. Para cada direção, o compute lê o vizinho axial anterior na mesma direção, adiciona fonte local e mistura laterais/oposto com `giTurn`/`giBackTurn`, aplicando `giDecay`.

O receptor amostra a célula do lado externo da superfície e vizinhos axiais; não usa taps diagonais.

## 10. Diagnóstico

Contadores atômicos registram:

- módulo encontrado;
- Build ID correto;
- hooks instalados;
- chamadas de detour;
- swaps/frames;
- shaderSource;
- shaders patchados;
- cache hits/misses.

`VoxelRuntime` fornece contador de dispatch. A cada 300 frames é emitido um heartbeat. Isso permite separar falha de integração de falha visual/matemática.
