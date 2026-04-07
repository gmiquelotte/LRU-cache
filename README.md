Relatório de Documentação da Cache LRU
	1.	Introdução
Este relatório documenta a implementação de uma Cache LRU em C. Seu objetivo é explicar a lógica de cada função e a organização das estruturas de dados, de modo a servir como referência para manutenção e para entendimento do código.
	2.	Estruturas de Dados

	•	NoCache: representa um nó da lista duplamente encadeada. Contém:
	•	chave (tipo chave_t, definido como int)
	•	valor (tipo valor_t, definido como char *, sendo uma string alocada)
	•	ponteiros proximo e anterior para conectar-se aos nós adjacentes na lista LRU.
	•	EntradaHash: representa uma entrada na tabela hash (separate chaining). Contém:
	•	chave (mesma chave do nó)
	•	ponteiro no para o NoCache correspondente na lista LRU
	•	ponteiro proximo para formar a lista de colisão dentro de um bucket.
	•	TabelaHash: contém:
	•	tamanho: número de buckets (inteiro)
	•	buckets: array de ponteiros a EntradaHash *, cada posição inicia uma lista ligada de entradas em caso de colisões.
	•	CacheLRU: estrutura principal da cache. Contém:
	•	capacidade: número máximo de itens que podem ser armazenados
	•	tamanho: número atual de itens
	•	tabela_hash: ponteiro para a TabelaHash usada para buscas rápidas por chave
	•	cabeca: ponteiro para o nó mais recentemente usado (head da lista)
	•	cauda: ponteiro para o nó menos recentemente usado (tail da lista)
A lista duplamente encadeada liga todos os nós atualmente na cache, permitindo mover rapidamente um nó para o início ou remover o último. A tabela hash permite localizar rapidamente, em tempo médio O(1), se uma chave está presente e obter o ponteiro ao nó correspondente.

	3.	Funções Auxiliares de Hash

	•	eh_primo(int n): determina se n é primo. Usa checagem de divisibilidade por 2 e 3 e depois incrementos de 6 em 6 (testando i e i+2) até √n. Usada para encontrar tamanho apropriado para buckets.
	•	proximo_primo(int n): a partir de n, encontra o próximo número primo ≥ n. Se n ≤ 2, retorna 2; senão, ajusta para ser ímpar e testa em loop até encontrar primo.
	•	hash_int(chave_t chave, int tamanho): converte a chave (int) para unsigned int e faz módulo pelo número de buckets, produzindo índice válido em [0, tamanho−1]. Converte para unsigned para lidar corretamente com valores negativos ou bit patterns.

	4.	Operações na Tabela Hash

	•	criar_tabela_hash(int num_buckets): aloca e inicializa uma TabelaHash. Primeiro aloca a struct, depois faz calloc no array buckets com num_buckets posições, já zeradas (NULL). Em falha de malloc ou calloc, imprime mensagem em stderr e libera o que já havia sido alocado, retornando NULL.
	•	destruir_tabela_hash(TabelaHash *th): percorre cada bucket e libera todas as estruturas EntradaHash em cada lista de colisão, sem tocar nos nós de dados (pois estes são liberados separadamente). Por fim, libera o array de buckets e a struct TabelaHash.
	•	buscar_entrada_hash(TabelaHash *th, chave_t chave): calcula índice via hash_int, percorre a lista ligada em buckets[idx] comparando eh->chave == chave. Retorna ponteiro à EntradaHash se encontrada, ou NULL caso não exista.
	•	inserir_entrada_hash(TabelaHash *th, chave_t chave, NoCache *no): calcula índice, aloca nova EntradaHash, atribui chave e ponteiro ao nó, insere-a no início da lista de colisão do bucket. Em falha de malloc, imprime erro e retorna 0; em sucesso, retorna 1.
	•	remover_entrada_hash(TabelaHash *th, chave_t chave): calcula índice, percorre lista do bucket mantendo ponteiro anterior; se encontra eh->chave == chave, ajusta ponteiros para retirar a entrada da lista e libera a struct EntradaHash; retorna 1 se removeu, 0 se não achou.

	5.	Operações na Lista Duplamente Encadeada (LRU)

	•	remove_no_da_lista(CacheLRU *cache, NoCache *no): remove um nó arbitrário da lista LRU. Ajusta:
	•	se no->anterior não for NULL, faz no->anterior->proximo = no->proximo; senão (no era head), atualiza cache->cabeca = no->proximo.
	•	se no->proximo não for NULL, faz no->proximo->anterior = no->anterior; senão (no era tail), atualiza cache->cauda = no->anterior.
	•	Finalmente, zera os ponteiros no->proximo e no->anterior para evitar ligações pendentes. Não faz free do nó nem do valor; apenas o desconecta.
	•	insere_no_no_head(CacheLRU *cache, NoCache *no): insere um nó isolado no início da lista, tornando-o o mais recentemente usado. Ajustes:
	•	no->proximo = cache->cabeca; no->anterior = NULL;
	•	Se havia cache->cabeca, seta cache->cabeca->anterior = no.
	•	Atualiza cache->cabeca = no.
	•	Se cache->cauda era NULL (lista vazia), também define cache->cauda = no.
	•	remove_tail(CacheLRU *cache): remove e retorna o nó menos recentemente usado (tail). Se cache->cauda for NULL, retorna NULL. Se houver mais de um nó, ajusta cache->cauda = tail->anterior e define cache->cauda->proximo = NULL; se era o único nó, define cache->cabeca = cache->cauda = NULL. Antes de retornar, zera tail->proximo e tail->anterior. Quem chama deve liberar o valor e o próprio nó.

	6.	Funções Públicas

	•	inicializar_cache(int capacidade):
	•	Se capacidade <= 0, retorna NULL.
	•	Aloca struct CacheLRU; em falha, imprime erro e retorna NULL.
	•	Define capacidade, tamanho = 0, cabeca = cauda = NULL.
	•	Calcula num_buckets = proximo_primo(capacidade * 2) para dimensionar tabela hash.
	•	Chama criar_tabela_hash(num_buckets); se falhar, libera a struct CacheLRU e retorna NULL.
	•	Retorna ponteiro à CacheLRU inicializada.
	•	valor_t obter_valor(CacheLRU *cache, chave_t chave):
	•	Se cache é NULL, retorna NULL.
	•	Busca entrada na hash: se não encontrada, retorna NULL.
	•	Se encontrada, obtém NoCache *no = eh->no. Se no não for já o head, chama remove_no_da_lista(cache, no) e insere_no_no_head(cache, no) para marcá-lo como mais recentemente usado.
	•	Retorna no->valor (ponteiro interno à string). Chamador não deve liberar esse ponteiro.
	•	void inserir_par(CacheLRU *cache, chave_t chave, const char *valor):
	•	Se cache ou valor for NULL, ignora e retorna.
	•	Busca na hash:
	•	Se já existe (eh != NULL): obtém nó, faz strdup(valor) para nova cópia de string; se strdup falhar, imprime erro e retorna (mantém valor antigo); senão, faz free(no->valor) e no->valor = nova_str; move o nó para head se não for já head; retorna.
	•	Se não existe:
	•	Aloca NoCache *novo; se malloc falhar, imprime erro e retorna.
	•	Faz strdup(valor) para str_copy; se falhar, imprime erro, libera novo, retorna.
	•	Atribui novo->chave = chave, novo->valor = str_copy, zera ponteiros.
	•	Se cache->tamanho >= cache->capacidade, chama remove_tail para obter nó antigo; se retornou nó (old), chama remover_entrada_hash(cache->tabela_hash, old->chave), faz free(old->valor) e free(old), decrementa cache->tamanho.
	•	Chama insere_no_no_head(cache, novo).
	•	Chama inserir_entrada_hash(cache->tabela_hash, chave, novo); se falhar, remove novo da lista com remove_no_da_lista, faz free(novo->valor) e free(novo), e retorna (estrutura anterior permanece consistente).
	•	Em caso de sucesso, incrementa cache->tamanho.
	•	void destruir_cache(CacheLRU *cache):
	•	Se cache for NULL, retorna sem crash.
	•	Percorre lista a partir de cache->cabeca, para cada nó: armazena prox = atual->proximo, faz free(atual->valor), free(atual), segue para prox. Após liberar todos,
	•	Chama destruir_tabela_hash(cache->tabela_hash) para liberar estruturas de hash.
	•	Faz free(cache).
	•	Após chamar, recomenda-se no código chamador atribuir ponteiro a NULL para evitar uso pós-free.

	7.	Gerenciamento de Memória e Falhas de Alocação

	•	Cada chamada a malloc, calloc ou strdup é verificada. Em caso de falha, imprime mensagem em stderr informando o local do erro.
	•	Em falha durante inserção, a implementação reverte alterações parciais: por exemplo, se já removeu LRU e depois falha ao inserir na hash, libera adequadamente o nó novo e seu valor, mas já removeu corretamente o antigo. Se falha antes de remover o antigo (por falta de memória ao criar o nó novo), nada altera na cache existente.
	•	As strings de valor são sempre copiadas internamente com strdup, isolando o armazenamento interno de possíveis mutações externas. Ao atualizar valor de chave existente, libera-se a string antiga antes de atribuir a nova.
	•	Ao remover um nó (seja LRU ou durante destruir), libera-se valor e depois o próprio nó.
	•	Na destruição da tabela hash, libera-se apenas as estruturas de EntradaHash; os ponteiros a nós já foram liberados ao percorrer a lista.
	•	Isso garante que não restem vazamentos nem ponteiros pendentes.

	8.	Complexidade

	•	Buscar (obter_valor): busca em hash em tempo médio O(1), mais operação O(1) para mover nó na lista.
	•	Inserir/Atualizar (inserir_par): busca em hash O(1) médio; se atualização, aloca nova string e move nó O(1); se inserção: potencial remoção de tail (O(1)), alocação de nó e string, inserção em head (O(1)), inserção em hash (O(1)). No geral, O(1) tempo médio.
	•	A qualidade de O(1) depende de fator de carga da hash estar controlado, por isso dimensionamos buckets como primo ≈ 2×capacidade, mantendo listas de colisão curtas.

	9.	Uso e Exemplos

	•	Arquivo main.c de demonstração inclui:
	•	Inicialização de cache com capacidade 3 e 1.
	•	Inserção de pares, busca de valores, atualização de valor, teste da política LRU ao inserir além da capacidade, substituição correta em capacidade 1.
	•	Impressão de resultados no console para verificação.
