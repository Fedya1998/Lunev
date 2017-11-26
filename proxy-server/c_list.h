//
// Created by fedya on 24.11.17.
//

#ifndef PROXY_SERVER_C_LIST_H
#define PROXY_SERVER_C_LIST_H

static FILE *DUMP_FILE = 0;

typedef struct list {
    struct listelem *first;
    struct listelem *last;
    size_t count;
    char name[32];
} List;

typedef struct listelem {
    void *data;
    struct listelem *previous;
    struct listelem *next;
    List *header;
} ListElem;

List *ListCtor();
void ListDtor(List *list);
int ListIsEmpty(List *list);
void Mini_Elem_Dump(ListElem * element);
void ListDump(List *list);
ListElem *AddAfter(ListElem *element, void *data);
void DeleteElem(ListElem *element);
void ListElemsDtor(ListElem *element);

List *ListCtor() {
    List *list = (List *) calloc(1, sizeof(List));
    return list;
}

void ListDtor(List *list){
    ListElemsDtor(list->first);
    free(list);
}

void ListElemsDtor(ListElem *element) {
    if (!element)
        return;
    if (element->next)
        ListElemsDtor(element->next);
    free(element);
}

int ListIsEmpty(List *list) {
    if (!list->count) {
        if (list->last || list->first) {
            fprintf(stderr, "PIZDEC CHO HAPPENED\n");
            exit(EXIT_FAILURE);
        } else
            return 1;
    } else
        return 0;
}

ListElem *AddAfter(ListElem *element, void *data) {
    ListElem *elem_now = (ListElem *) calloc(1, sizeof(ListElem));
    elem_now->data = data;
    if (!element) {
        List *list = ListCtor();
        list->last = elem_now;
        list->first = elem_now;
        list->count++;
        elem_now->header = list;
    } else {
        elem_now->next = element->next;
        elem_now->previous = element;
        element->next = elem_now;
        elem_now->header = element->header;
        elem_now->header->count++;
        if (!elem_now->next)
            elem_now->header->last = elem_now;
    }
    return elem_now;
}

void DeleteElem(ListElem *element) {
    if (element->previous)
        element->previous->next = element->next;
    else
        element->header->first = element->next;
    if (element->next)
        element->next->previous = element->previous;
    else
        element->header->last = element->previous;
    element->header->count--;
    free(element);
}

void ListDump(List *list){
    char name_to_open[64] = "";
    sprintf(name_to_open, "%s_dump.gv", list->name);
    DUMP_FILE = fopen(name_to_open, "w");
    fprintf(DUMP_FILE, "digraph G{\n");
    ListElem * element = list->first;
    while(1){
        if (!element)
            break;
        Mini_Elem_Dump(element);
        fprintf(DUMP_FILE, "->");
        Mini_Elem_Dump(element->next);
        element = element->next;
    }
    fprintf(DUMP_FILE, "}");
    fclose(DUMP_FILE);

    char cmd[128] = "dot -Tpng ";
    sprintf(cmd + strlen(cmd), "%s_dump.gv -o %s_dump.png", list->name, list->name);
    system(cmd);
}

void Mini_Elem_Dump(ListElem * element){
    if (!element){
        fprintf(DUMP_FILE, "nul");
        return;
    }
    fprintf(DUMP_FILE, "%celement %p\ndata %p\nheader %p\nnext %p\n", 34,
            element, element->data,
            element->header, element->next);
    char buf_dump[256] = "";
    BufDump(element->data, buf_dump);
    fprintf(DUMP_FILE, "%s", buf_dump);
    fprintf(DUMP_FILE, "%c", 34);
}

#endif //PROXY_SERVER_C_LIST_H
