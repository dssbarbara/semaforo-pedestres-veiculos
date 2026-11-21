# Semáforo de Pedestres Embarcado (Zephyr RTOS + FRDM-KL25Z)

Controle de um semáforo de pedestres (LED verde e LED vermelho) usando **duas threads independentes** e **mutex** para garantir exclusão mútua. O funcionamento foi validado com testes baseados no **Modelo V** (unidade, integração e sistema) e revisado por pares.

- **Autor:** _Barbara_
- **Avaliador (dupla):** _Giovanna_

## 1. Requisitos

| ID | Requisito |
|---|---|
| R1 | O LED verde acende por 4 s. |
| R2 | O LED vermelho acende por 4 s. |
| R3 | Verde e vermelho são controlados por threads independentes. |
| R4 | Verde e vermelho nunca ficam acesos ao mesmo tempo (exclusão mútua via mutex). |

## 2. Como o código funciona

Tudo está em `src/main.c`.

- `green_thread` e `red_thread` fazem o mesmo ciclo em loop: trava o mutex, acende seu LED, espera 4 s (`k_msleep`), apaga o LED, libera o mutex.
- O **mutex** `ped_mutex` fica travado durante todo o tempo em que um LED está aceso. A outra thread bloqueia em `k_mutex_lock` até que ele seja liberado, o que garante a alternância sem sobreposição.
- Cada thread confere, logo depois de acender, se o LED da outra está aceso. Se estiver, imprime `ERRO: exclusao mutua violada`.
- `printk` registra cada `ON` e `OFF` com o tempo em ms (`k_uptime_get`), usado como evidência nos testes.
- Os `#define TEST_ONLY_GREEN` e `TEST_ONLY_RED` usam `#ifndef` para compilar só uma das threads, o que permite o teste de unidade isolado.

## T1.1: Unidade, thread verde (R1, R3)

- **Pré-condição:** `#define TEST_ONLY_GREEN` ativo; placa reiniciada.
- **Etapas:** gravar, abrir o monitor serial e observar os logs.
- **Pós-condição esperada:** verde aceso por ≈ 4000 ms; vermelho nunca acende; nenhum `ERRO`.
- **Resultado:** **PASS**.

```
VERDE OFF t=28024
VERDE ON  t=28026
VERDE OFF t=32028
VERDE ON  t=32029
VERDE OFF t=36031
VERDE ON  t=36032
```

Duração do verde aceso (ON → OFF): 28026→32028 = 4002 ms, 32029→36031 = 4002 ms, 36032→40034 = 4002 ms, 40036→44038 = 4002 ms.

> Nota: o LED parece sempre aceso porque, sem a thread vermelha, o verde volta a acender 1 a 2 ms depois de apagar. O log confirma que o ciclo está correto.

### T1.2: Unidade, thread vermelha (R2, R3)

- **Pré-condição:** `#define TEST_ONLY_RED` ativo; placa reiniciada.
- **Etapas:** gravar, abrir o monitor serial e observar os logs.
- **Pós-condição esperada:** vermelho aceso por ≈ 4000 ms; verde nunca acende; nenhum `ERRO`.
- **Resultado:** **PASS**, depois de corrigir o mapeamento do LED (o `led1` acendia o azul).

```
VERMELHO OFF t=40049
VERMELHO ON  t=40051
VERMELHO OFF t=44053
VERMELHO ON  t=44055
VERMELHO OFF t=48057
VERMELHO ON  t=48059
```

Duração do vermelho aceso: 40051→44053 = 4002 ms, 44055→48057 = 4002 ms, 48059→52061 = 4002 ms, 52062→56064 = 4002 ms.

### T2.1: Integração, duas threads com mutex (R1 a R4)

- **Pré-condição:** nenhum `TEST_ONLY_...` ativo; mutex habilitado.
- **Etapas:** gravar, reiniciar a placa e observar os logs por pelo menos 1 minuto.
- **Pós-condição esperada:** alternância verde → vermelho → verde, cada cor por ≈ 4 s, sem `ERRO`.
- **Resultado:** **PASS**.

```
VERDE ON  t=4
VERDE OFF t=4005
VERMELHO ON  t=4006
VERMELHO OFF t=8008
VERDE ON  t=8010
VERDE OFF t=12012
VERMELHO ON  t=12013
VERMELHO OFF t=16015
VERDE ON  t=16017
VERDE OFF t=20019
```

Cada fase dura entre 4001 e 4002 ms. A troca de uma cor para outra leva 1 a 2 ms (por exemplo, `VERDE OFF t=4005` → `VERMELHO ON t=4006`).

### T2.2: Integração, teste negativo sem mutex (R4)

- **Objetivo:** provar que o mutex é necessário.
- **Pré-condição:** chamadas `k_mutex_lock` e `k_mutex_unlock` comentadas nas duas threads (alteração temporária, já revertida).
- **Etapas:** gravar e observar os logs.
- **Pós-condição esperada:** a exclusão mútua é violada e as mensagens de erro aparecem.
- **Resultado:** **PASS** (a falha esperada foi observada).

```
VERDE OFF t=32056
VERDE ON  t=32057
ERRO: exclusao mutua violada (verde+vermelho)
VERMELHO OFF t=32074
VERMELHO ON  t=32076
ERRO: exclusao mutua violada (verde+vermelho)
```

Sem o mutex as threads acendem os LEDs ao mesmo tempo, e o código de verificação detecta a violação. 

### T3.1: Sistema, placa real com cronômetro (R1 a R4)

- **Pré-condição:** código final (sem `TEST_ONLY_...`) gravado; placa alimentada pela USB.
- **Etapas:** reiniciar a placa pelo botão reset e cronometrar 3 ciclos completos (verde + vermelho).
- **Pós-condição esperada:** cada cor dura ≈ 4 s; o ciclo recomeça do verde após o reset; sem travamentos.
- **Resultado:** **PASS** 

| Ciclo | Verde (s) | Vermelho (s) |
|---|---|---|
| 1 | 4 | 4 |
| 2 | 4 | 4 |
| 3 | 4 | 4 |

## 5. Resumo dos resultados

| Teste | Nível | Requisitos | Resultado |
|---|---|---|---|
| T1.1 | Unidade | R1, R3 | PASS |
| T1.2 | Unidade | R2, R3 | PASS |
| T2.1 | Integração | R1 a R4 | PASS |
| T2.2 | Integração (negativo) | R4 | PASS (falha esperada observada) |
| T3.1 | Sistema | R1 a R4 | PASS |

## 6. Avaliação por pares

O avaliador clonou o repositório, gravou o código na própria placa e repetiu os testes T1.1, T1.2, T2.1, T2.2 e T3.1 seguindo as pré-condições e etapas acima.

- **Avaliador:** _(nome)_

| Teste | Resultado do avaliador | Observações |
|---|---|---|
| T1.1 | _ | _ |
| T1.2 | _ | _ |
| T2.1 | _ | _ |
| T2.2 | _ | _ |
| T3.1 | _ | _ |

**Feedback do avaliador:** _(falhas lógicas, comportamentos não previstos, sugestões)_
