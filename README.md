# Emulador de Game Boy

A ideia desse projeto surgiu nas férias entre o segundo e o terceiro semestre, fomentada principalmente por [este vídeo](https://www.youtube.com/watch?v=hy2yY5a1Z-0).

O objetivo principal deste projeto foi compreender melhor como funciona a arquitetura básica de um console clássico e como as suas diferentes peças (CPU, PPU, Memória) se comunicam.

## Arquitetura e Implementação

### CPU e Instruções

As instruções da CPU foram implementadas seguindo a **Tabela Principal de Opcodes** e, para as instruções estendidas com o prefixo `0xCB`, foi utilizada a **Tabela de Opcodes CB**. Ambas as tabelas podem ser acessadas [nesta página (Meganesu's GB Opcodes)](https://meganesu.github.io/generate-gb-opcodes/).

### Memória

O funcionamento da memória foi implementado utilizando arrays com tamanhos de bytes rigorosamente definidos, visando otimizar ao máximo o processamento e emular fielmente o mapeamento de memória original do hardware.

## Como usar

O programa deve ser executado através da linha de comando, informando o caminho do arquivo da ROM que deseja jogar:

```bash
./gb_emulator "caminho/para/a/rom.gb"
```

_(Nota: Certifique-se de que o emulador foi compilado previamente usando GCC e a biblioteca SDL2)._

## Ferramentas e Referências

- **Documentação Oficial (Pan Docs):** [gbdev.io/pandocs](https://gbdev.io/pandocs/)
- **Documentação RGBDS:** [rgbds.gbdev.io/docs](https://rgbds.gbdev.io/docs/)
- **Biblioteca de Vídeo/Input:** [SDL2](https://wiki.libsdl.org/SDL2/)
- **Revisão e Correções:** Gemini CLI
