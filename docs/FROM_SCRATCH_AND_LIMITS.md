# Origem do código e limites técnicos

## Escrito do zero

Os arquivos em `src/` e `include/lvsgi/` desta entrega foram criados para este projeto. Nenhum `.cpp/.hpp` de versões anteriores do mod foi copiado como base.

Os templates da biblioteca foram usados somente para decisões de compilação/empacotamento: CMake, NDK ARM64, `preload-native`, `PL_REGISTER_MOD`, alinhamento de página e estrutura do `.levipack`.

O código GLSL injetado é novo. O pacote Java enviado foi usado para entender a semântica de floodfill/faces, não para copiar seus shaders para o Bedrock.

## Limites

1. Não existe garantia absoluta de zero tempo no primeiro programa que seja simultaneamente novo, não pré-aquecido e ausente do cache. O driver ainda precisa produzir código de máquina.
2. O prewarm cobre as 220 combinações únicas extraídas dos materiais ItemInHand enviados. Uma atualização do Minecraft pode mudar esse conjunto; use as ferramentas em `tools/` para regenerar perfil/prewarm.
3. O volume mundial recebe geometria que passa pelos shaders compatíveis. Geometria nunca enviada ao pipeline gráfico ainda não pode aparecer magicamente no SSBO.
4. Remoção de blocos fora da região observada pode deixar uma célula antiga até ela ser sobrescrita/revalidada pelo ring. Uma integração futura com eventos internos de chunk permitiria invalidação instantânea, mas esta versão evita offsets/ABI internos não verificados.
5. A execução real em Android/RenderDragon deve ser validada no dispositivo porque este ambiente não executa Minecraft Bedrock nem o driver GLES do aparelho.
