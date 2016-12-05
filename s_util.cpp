#include <string.h>

#include "s_util.h"
#include "s_parser.h"

#include "u_assert.h"

namespace s {

///! SourceRecord
SourceRecord *SourceRecord::m_record = nullptr;

void SourceRecord::registerSource(SourceRange source,
                                  const char *name,
                                  int rowBegin,
                                  int colBegin)
{
    SourceRecord *record = new SourceRecord;
    record->m_prev = m_record;
    record->m_source = source;
    record->m_name = name;
    record->m_rowBegin = rowBegin;
    record->m_colBegin = colBegin;
    m_record = record;
}

bool SourceRecord::findSourcePosition(char *source,
                                      const char **name,
                                      SourceRange *line,
                                      int *rowBegin,
                                      int *colBegin)
{
    SourceRecord *record = m_record;
    while (record) {
        if (source >= record->m_source.m_begin && source < record->m_source.m_end) {
            *name = record->m_name;
            int rowCount = 0;
            SourceRange lineSearch = { record->m_source.m_begin, record->m_source.m_begin };
            while (lineSearch.m_begin < record->m_source.m_end) {
                while (lineSearch.m_end < record->m_source.m_end && *lineSearch.m_end != '\n')
                    lineSearch.m_end++;
                if (lineSearch.m_end < record->m_source.m_end)
                    lineSearch.m_end++;
                if (source >= lineSearch.m_begin && source < lineSearch.m_end) {
                    int colCount = source - lineSearch.m_begin;
                    *line = lineSearch;
                    *rowBegin = rowCount + record->m_rowBegin;
                    *colBegin = colCount + ((rowCount == 0) ? record->m_colBegin : 0);
                    return true;
                }
                lineSearch.m_begin = lineSearch.m_end;
                rowCount++;
            }
            U_ASSERT(0 && "text in range but not in any line");
        }
        record = record->m_prev;
    }
    return false;
}

///! FileRange
void FileRange::recordStart(char *text, FileRange *range) {
    SourceRange line;
    Parser::consumeFiller(&text);
    range->m_textFrom = text;
    bool found = SourceRecord::findSourcePosition(text,
                                                  (const char **)&range->m_file,
                                                  &line,
                                                  &range->m_rowFrom,
                                                  &range->m_colFrom);
    (void)found;
    U_ASSERT(found);
}

void FileRange::recordEnd(char *text, FileRange *range) {
    SourceRange line;
    const char *file;
    range->m_textTo = text;
    bool found = SourceRecord::findSourcePosition(text,
                                                  &file,
                                                  &line,
                                                  &range->m_rowTo,
                                                  &range->m_colTo);
    (void)found;
    U_ASSERT(found);
    U_ASSERT(strcmp(file, range->m_file) == 0);
}

size_t djb2(const char *str, size_t length) {
    size_t hash = 5381;
    for (size_t i = 0; i < length; i++) {
        int ch = str[i];
        hash = ((hash << 5) + hash) + ch;
    }
    return hash;
}


}
