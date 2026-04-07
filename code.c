// cache_lru.c
#include "cache_lru.h"

// Funções auxiliares para hash
// Testar primalidade simples (para tamanho pequeno) e achar próximo primo >= n
static int eh_primo(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (int i = 5; i * (long)i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}

static int proximo_primo(int n) {
    if (n <= 2) return 2;
    int candidate = (n % 2 == 0) ? n + 1 : n;
    while (!eh_primo(candidate)) {
        candidate += 2;
    }
    return candidate;
}

// Função de hash para chave_t (int). Retorna índice [0, tamanho-1].
static int hash_int(chave_t chave, int tamanho) {
    // Converte para unsigned para lidar com negativos de forma bem definida
    unsigned int u = (unsigned int)chave;
    return u % (unsigned int)tamanho;
}

// Cria uma tabela hash com dado número de buckets; retorna NULL em falha.
static TabelaHash* criar_tabela_hash(int num_buckets) {
    TabelaHash *th = malloc(sizeof(TabelaHash));
    if (!th) {
        fprintf(stderr, "Erro: malloc falhou em criar_tabela_hash\n");
        return NULL;
    }
    th->tamanho = num_buckets;
    th->buckets = calloc(num_buckets, sizeof(EntradaHash*));
    if (!th->buckets) {
        fprintf(stderr, "Erro: calloc falhou em criar buckets da hash\n");
        free(th);
        return NULL;
    }
    return th;
}

// Libera toda a tabela hash (supondo que as EntradasHash devem ser liberadas,
// mas NÃO libera os NoCache apontados; quem chamou libera nós separadamente).
static void destruir_tabela_hash(TabelaHash *th) {
    if (!th) return;
    for (int i = 0; i < th->tamanho; i++) {
        EntradaHash *eh = th->buckets[i];
        while (eh) {
            EntradaHash *prox = eh->proximo;
            // Não liberar eh->no aqui (liberado em lista LRU)
            free(eh);
            eh = prox;
        }
    }
    free(th->buckets);
    free(th);
}

// Busca uma entrada na hash: retorna o ponteiro à EntradaHash se achar, ou NULL.
// Também pode retornar ponteiro ao ponteiro anterior para remoção, mas aqui só busca.
static EntradaHash* buscar_entrada_hash(TabelaHash *th, chave_t chave) {
    int idx = hash_int(chave, th->tamanho);
    EntradaHash *eh = th->buckets[idx];
    while (eh) {
        if (eh->chave == chave) {
            return eh;
        }
        eh = eh->proximo;
    }
    return NULL;
}

// Insere uma nova entrada (chave -> nó) na hash (no início da lista de bucket).
// Retorna 1 em sucesso, 0 em falha.
static int inserir_entrada_hash(TabelaHash *th, chave_t chave, NoCache *no) {
    int idx = hash_int(chave, th->tamanho);
    EntradaHash *eh = malloc(sizeof(EntradaHash));
    if (!eh) {
        fprintf(stderr, "Erro: malloc falhou em inserir_entrada_hash\n");
        return 0;
    }
    eh->chave = chave;
    eh->no = no;
    eh->proximo = th->buckets[idx];
    th->buckets[idx] = eh;
    return 1;
}

// Remove a entrada para chave na hash e libera a estrutura EntradaHash.
// Retorna 1 se removeu, 0 se não achou.
static int remover_entrada_hash(TabelaHash *th, chave_t chave) {
    int idx = hash_int(chave, th->tamanho);
    EntradaHash *eh = th->buckets[idx];
    EntradaHash *ant = NULL;
    while (eh) {
        if (eh->chave == chave) {
            if (ant) {
                ant->proximo = eh->proximo;
            } else {
                th->buckets[idx] = eh->proximo;
            }
            free(eh);
            return 1;
        }
        ant = eh;
        eh = eh->proximo;
    }
    return 0;
}

// Funções auxiliares para lista duplamente encadeada LRU
// Remove nó arbitrário da lista (ajusta head/cauda). Não libera o nó nem o valor aqui.
// Retorna void. Supõe que no != NULL e que pertence à lista.
static void remove_no_da_lista(CacheLRU *cache, NoCache *no) {
    if (!no) return;
    if (no->anterior) {
        no->anterior->proximo = no->proximo;
    } else {
        // era head
        cache->cabeca = no->proximo;
    }
    if (no->proximo) {
        no->proximo->anterior = no->anterior;
    } else {
        // era cauda
        cache->cauda = no->anterior;
    }
    no->proximo = no->anterior = NULL;
}

// Insere nó no início (head) da lista. Ajusta head/cauda. Supõe no não esteja em nenhuma lista.
static void insere_no_no_head(CacheLRU *cache, NoCache *no) {
    no->proximo = cache->cabeca;
    no->anterior = NULL;
    if (cache->cabeca) {
        cache->cabeca->anterior = no;
    }
    cache->cabeca = no;
    if (!cache->cauda) {
        // Lista antes vazia
        cache->cauda = no;
    }
}

// Remove o nó menos recentemente usado (tail) da lista, ajusta ponteiros, e retorna pointer ao nó removido.
// Se lista vazia, retorna NULL. A liberação de memória do nó e do valor ocorre quem chamar.
static NoCache* remove_tail(CacheLRU *cache) {
    NoCache *tail = cache->cauda;
    if (!tail) return NULL;
    if (tail->anterior) {
        cache->cauda = tail->anterior;
        cache->cauda->proximo = NULL;
    } else {
        // Só havia um elemento
        cache->cabeca = cache->cauda = NULL;
    }
    tail->anterior = tail->proximo = NULL;
    return tail;
}

// Implementações das funções públicas:

CacheLRU* inicializar_cache(int capacidade) {
    if (capacidade <= 0) return NULL;
    CacheLRU *cache = malloc(sizeof(CacheLRU));
    if (!cache) {
        fprintf(stderr, "Erro: malloc falhou em inicializar_cache\n");
        return NULL;
    }
    cache->capacidade = capacidade;
    cache->tamanho = 0;
    cache->cabeca = cache->cauda = NULL;
    // Dimensionar tabela hash: por ex. próximo primo >= 2 * capacidade
    int num_buckets = proximo_primo(capacidade * 2);
    cache->tabela_hash = criar_tabela_hash(num_buckets);
    if (!cache->tabela_hash) {
        // falha ao alocar hash: liberar cache e retornar NULL
        free(cache);
        return NULL;
    }
    return cache;
}

valor_t obter_valor(CacheLRU *cache, chave_t chave) {
    if (!cache) return NULL;
    EntradaHash *eh = buscar_entrada_hash(cache->tabela_hash, chave);
    if (!eh) {
        // não encontrado
        return NULL;
    }
    NoCache *no = eh->no;
    // move para head (mais recentemente usado)
    // se já for head, nada a fazer
    if (cache->cabeca != no) {
        remove_no_da_lista(cache, no);
        insere_no_no_head(cache, no);
    }
    return no->valor;
}

void inserir_par(CacheLRU *cache, chave_t chave, const char *valor) {
    if (!cache || !valor) return;
    // Verificar se já existe
    EntradaHash *eh = buscar_entrada_hash(cache->tabela_hash, chave);
    if (eh) {
        // Já existe: atualizar valor
        NoCache *no = eh->no;
        // Alocar nova cópia da string
        char *nova_str = strdup(valor);
        if (!nova_str) {
            fprintf(stderr, "Erro: strdup falhou em inserir_par (atualizar valor)\n");
            return; // não corrompe a estrutura; mantém valor antigo
        }
        // Liberar string antiga e atribuir nova
        free(no->valor);
        no->valor = nova_str;
        // Move para head
        if (cache->cabeca != no) {
            remove_no_da_lista(cache, no);
            insere_no_no_head(cache, no);
        }
        return;
    }
    // Não existe: criar novo nó
    NoCache *novo = malloc(sizeof(NoCache));
    if (!novo) {
        fprintf(stderr, "Erro: malloc falhou em inserir_par (NoCache)\n");
        return;
    }
    // Copiar string valor
    char *str_copy = strdup(valor);
    if (!str_copy) {
        fprintf(stderr, "Erro: strdup falhou em inserir_par (valor)\n");
        free(novo);
        return;
    }
    novo->chave = chave;
    novo->valor = str_copy;
    novo->proximo = novo->anterior = NULL;
    // Se cache cheia, remover LRU
    if (cache->tamanho >= cache->capacidade) {
        // Remover tail
        NoCache *old = remove_tail(cache);
        if (old) {
            // Remover da hash
            int rem = remover_entrada_hash(cache->tabela_hash, old->chave);
            (void)rem; // poderíamos verificar, mas assumimos que existia
            // Liberar memória do nó antigo
            free(old->valor);
            free(old);
            cache->tamanho--;
        }
    }
    // Inserir novo nó em head
    insere_no_no_head(cache, novo);
    // Inserir na hash; se falhar, desfazer inserção na lista e liberar
    if (!inserir_entrada_hash(cache->tabela_hash, chave, novo)) {
        // falha ao inserir na hash: remover nó recém-inserido da lista e liberar
        remove_no_da_lista(cache, novo);
        free(novo->valor);
        free(novo);
        return;
    }
    cache->tamanho++;
}

void destruir_cache(CacheLRU *cache) {
    if (!cache) return;
    // Primeiro, liberar todos nós da lista duplamente encadeada
    NoCache *atual = cache->cabeca;
    while (atual) {
        NoCache *prox = atual->proximo;
        // Liberar string
        free(atual->valor);
        // Liberar nó
        free(atual);
        atual = prox;
    }
    // Liberar tabela hash (entradas). As entradas apontam para nós já liberados, mas aqui só liberamos as estruturas de EntradaHash.
    destruir_tabela_hash(cache->tabela_hash);
    // Liberar CacheLRU
    free(cache);
}
