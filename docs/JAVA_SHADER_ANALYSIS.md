# Análise da GI/faces do shader Java enviado

Arquivos principais analisados no pacote `ForkReimaginedGI_AquaticUpdate(2).zip`:

- `shaders/program/shadowcomp.glsl`
- `shaders/lib/lighting/mainLighting.glsl`
- `shaders/lib/voxelization/reflectionVoxelData.glsl`

## Floodfill

`GetLightCalculated()` consulta somente os seis vizinhos axiais: +X, -X, +Y, -Y, +Z e -Z. A soma é dividida por 6,42, deliberadamente um pouco acima de seis, para que a energia diminua enquanto se propaga. Não há vizinhos diagonais nesse passo.

## Fonte solar por face

`GetSolarGISource()` só cria a fonte em uma célula hospedeira de ar/água. Em seguida percorre `faceOffsets[6]`. Para cada vizinho refletor/água:

1. calcula a normal daquela face a partir do offset;
2. calcula incidência solar daquela face;
3. testa visibilidade/sombra da face;
4. obtém albedo/tinta daquela superfície;
5. acumula a contribuição daquela face.

Isso é importante: a fonte não é decidida apenas pelo ID do voxel. A orientação da face participa da decisão.

## Dados de reflexão por face

`reflectionVoxelData.glsl` possui armazenamento indexado por face. Quando uma geometria pede replicação em várias faces, o código só copia para uma face se a célula vizinha naquela direção estiver vazia. Caso contrário grava apenas no índice da face correspondente à normal real.

## Limitação do volume escalar original

Depois da injeção, o floodfill Java vira um volume escalar RGBA. `mainLighting.glsl` comenta explicitamente que esse volume já não possui direção incidente confiável; por isso aplica um teste de hemisfério no receptor em vez de tentar reconstruir direção a partir do gradiente.

## Adaptação neste mod

A nova implementação preserva a direção que o volume Java perde:

- seis radiâncias RGB por voxel;
- uma para cada direção axial;
- fonte de superfície guardada separadamente por face;
- transporte reto com peso 1;
- mudança para uma direção lateral com `giTurn`;
- retorno para a direção oposta com `giBackTurn`;
- decaimento por `giDecay`.

Logo, a radiância que saiu pela face +X de um bloco continua identificável enquanto percorre o volume, e o receptor usa somente os canais cujo sentido realmente chega ao seu hemisfério.

## beta.5: topology corrected to match the Java floodfill

The Java reference does not attach propagated GI to the solid voxel itself. `GetSolarGISource` runs for a host cell and inspects the six neighboring voxels with the fixed offsets `+X,-X,+Y,-Y,+Z,-Z`; the reflecting surface normal is the opposite of that offset. `GetLightCalculated` then gathers exactly the six axial neighbors and divides by 6.42. `GetComplexLightVolume` explicitly rejects corner interpolation and only accepts an edge path when at least one corresponding axial path is open.

beta.5 therefore changed the Android implementation from "source stored on solid, discovered later" to "face source injected into the adjacent air voxel". Each air voxel retains six directional RGB channels, so +X and +Y faces of the same block remain different after injection. Propagation is axial only; straight travel dominates, perpendicular transfer uses `giTurn`, reversal uses `giBackTurn`, and receiver filtering never samples diagonal corners.
