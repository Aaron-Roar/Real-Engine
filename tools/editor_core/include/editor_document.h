#ifndef ROHR_EDITOR_DOCUMENT_H
#define ROHR_EDITOR_DOCUMENT_H

#include "editor_project.h"

#define EDITOR_DOCUMENT_PATH_MAX 1024

typedef struct EditorDocument {
    EditorProject *project;
    char path[EDITOR_DOCUMENT_PATH_MAX];
    bool dirty;
} EditorDocument;

EditorResult editor_document_create(EditorDocument *document);
EditorResult editor_document_load(EditorDocument *document, const char *path);
EditorResult editor_document_save(EditorDocument *document);
EditorResult editor_document_save_as(EditorDocument *document, const char *path);
EditorProject *editor_document_project_get(EditorDocument *document);
const EditorProject *editor_document_project_const_get(const EditorDocument *document);
void editor_document_destroy(EditorDocument *document);

#endif
