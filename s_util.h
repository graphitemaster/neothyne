#ifndef S_UTIL_HDR
#define S_UTIL_HDR

namespace s {

struct SourceRange {
    char *m_begin;
    char *m_end;
};

struct SourceRecord {
    static void registerSource(SourceRange source,
                               const char *name,
                               int rowBegin,
                               int colBegin);

    static bool findSourcePosition(char *source,
                                   const char **name,
                                   SourceRange *line,
                                   int *rowBegin,
                                   int *colBegin);

    static void start(char *text, SourceRange *range);
    static void end(char *text, SourceRange *range);

private:
    SourceRecord *m_prev;
    SourceRange m_source;
    const char *m_name;
    int m_rowBegin;
    int m_colBegin;

    static SourceRecord *m_record;
};

// Every instruction is annotated with a FileRange so that backtraces and
// profiling and debugging is possible
struct FileRange {
    char *m_file;
    char *m_textFrom;
    char *m_textTo;
    int m_rowFrom;
    int m_colFrom;
    int m_rowTo;
    int m_colTo;
    int m_lastCycleSeen;

    static void recordStart(char *text, FileRange *range);
    static void recordEnd(char *text, FileRange *range);
};

}

#endif
