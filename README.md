# Redes_TP1_Parte2

Redes de Computadores - TM

Lara Strutz Carvalho

Este repositório contém o código-fonte e os resultados da segunda parte do trabalho prático 1, focado em controle de congestionamento do protocolo TCP.

O projeto utiliza o simulador de redes **NS-3 (versão 3.36.1)** para realizar uma análise comparativa detalhada entre dois importantes algoritmos TCP:
* **TCP NewReno:** Um algoritmo clássico, baseado em perdas.
* **TCP CUBIC:** O algoritmo padrão em sistemas Linux modernos, otimizado para redes de alta performance.

A análise foca em métricas de desempenho sob diferentes condições de rede, investigando a evolução da janela de congestionamento (CWND), a vazão útil (goodput) e a justiça de alocação de banda entre fluxos com diferentes tempos de ida e volta (RTT).

## Estrutura do Repositório
* **/src**: Contém os arquivos de código-fonte em C++ (`.cc`) para cada parte do laboratório.
* **/results**: Contém resultados do trabalho (gráficos) organizados em subpastas por parte.

## Principais Conclusões do Estudo

A simulação demonstrou de forma conclusiva a superioridade do TCP CUBIC em cenários de redes modernas:

* **Eficiência em Redes de Alto BDP:** O CUBIC utiliza a largura de banda de forma muito mais eficiente em redes com alta capacidade e latência, mantendo uma janela de congestionamento maior e mais estável.
* **Resiliência à Latência e a Erros:** A performance do CUBIC é significativamente menos afetada pelo aumento do atraso da rede e por altas taxas de perda de pacotes em comparação com o NewReno.
* **Justiça de RTT:** O CUBIC provou ser justo na alocação de banda entre fluxos com diferentes RTTs, uma característica crucial para a equidade na Internet, enquanto o NewReno se mostrou fundamentalmente injusto.

## Como Compilar e Executar

1. Tenha uma instalação funcional do NS-3 (versão 3.36.1) em um ambiente Linux.
2.  Copie os arquivos da pasta `/source` para o diretório `scratch/` da sua instalação do NS-3.
3.  Abra um terminal na pasta raiz do NS-3.
4.  Execute os scripts de simulação usando o comando `./ns3 run`, passando os parâmetros desejados.
