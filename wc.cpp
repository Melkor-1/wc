#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <locale>
#include <system_error>

#include <clocale>
#include <cstdint>
#include <cstdlib>

#include <getopt.h>
#include <unistd.h>

namespace fs = std::filesystem;

struct Options {
    bool count_bytes {false};
    bool count_lines {false};
    bool count_words {false};
    bool count_max_line_length {false};
};

struct FileStatistics {
    std::uintmax_t lines {0};
    std::uintmax_t words {0};
    std::uintmax_t bytes {0};
    std::uintmax_t max_line_length {0};
};

enum class ParseOptionsError { 
    help_requested, 
    unknown_option 
};

static auto help(std::ostream& os, std::string_view argv0) -> void 
{
    os << std::format(R"(USAGE
    {} [OPTION]... [FILE]...

NAME
    {} - word, line, and byte count.

DESCRIPTION
    Print line, word, and byte counts for each FILE, and a total line if more
    than one FILE is specified. A line is defined as a string of characters
    delimited by a <newline> character, and a word is defined as a non-zero-
    -length sequence of printable characters delimited by white space. 

    When an option is specified, {} only reports the information requested by
    that option. The default action is equivalent to all the flags -clw having
    been specified.

    When no FILE, or when FILE is -, read standard input.

    If more than one input file is specified, a line of cumulative counts for
    all the files is displayed on a separate line after the output for the last
    file.

    By default, the standard output containts a line for each input file of the 
    form:
        lines    words    bytes    file_name

OPTION:
    -c, --bytes                 print the byte counts.
    -l, --lines                 print the newline counts.
    -L, --max-line-length       print the maximum display width.
    -w, --words                 print the word counts.
    -h, --help                  display this help and exit.

EXIT STATUS:
    The {} utility exits with 0 on success, or elsewise if an error occurs.
)",
                      argv0, argv0, argv0, argv0);
}

static auto usage_err(std::ostream& os, std::string_view argv0) -> void 
{
    os << "The syntax of the command is incorrect.\nTry " << argv0
       << " -h for more information.\n";
}

static auto read_err(std::ostream& os, const std::string_view& file) -> void 
{
    os << std::format(
        "error: failed to process '{}': {}\n", file,
        std::error_code {errno, std::generic_category()}.message());
}

static auto chkd_add(std::uintmax_t& res, std::uintmax_t a, std::uintmax_t b)
    -> bool {
    if (a + b < a) {
        return false;
    }

    res = a + b;
    return true;
}

static auto write_counts(std::ostream& os, 
                         const Options& options,
                         const FileStatistics& stats, 
                         const char* file) -> void 
{
    /* Currently, we are using the output formatting of System V version of wc:
     * "%7d%7d%7d %s\n", albeit with 2 added spaces before each field.
     *
     * TODO: Format these dynamically like wc does, to better align all lines. */
    if (options.count_lines) {
        os << std::format("  {:>7L}", stats.lines);
    }

    if (options.count_words) {
        os << std::format("  {:>7L}", stats.words);
    }

    if (options.count_bytes) {
        os << std::format("  {:>7L}", stats.bytes);
    }

    if (options.count_max_line_length) {
        os << std::format("  {:>7L}", stats.max_line_length);
    }

    if (file) {
        os << std::format("  {}", file);
    }

    /* Ensure lines are written atomically and immediately so that processes
     * running in parallel do not intersperse their output. */
    os << '\n' << std::flush;
}

[[nodiscard]] static auto parse_options(int argc, char* argv[])
    -> std::expected<Options, ParseOptionsError> 
{
    auto options {Options {}};

    static constexpr const ::option long_options[] {
        {"bytes", no_argument, nullptr, 'c'},
        {"lines", no_argument, nullptr, 'l'},
        {"max-line-length", no_argument, nullptr, 'L'},
        {"words", no_argument, nullptr, 'w'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}};

    while (true) {
        const int c {::getopt_long(argc, argv, "clLwh", long_options, nullptr)};

        if (c == -1) {
            break;
        }

        switch (c) {
        case 'c':
            options.count_bytes = true;
            break;

        case 'l':
            options.count_lines = true;
            break;

        case 'L':
            options.count_max_line_length = true;
            break;

        case 'w':
            options.count_words = true;
            break;

        case 'h':
            return std::unexpected {ParseOptionsError::help_requested};

            /* case '?' */
        default:
            return std::unexpected {ParseOptionsError::unknown_option};
        }
    }
    return options;
}

[[nodiscard]] static auto wc(const Options& options, std::istream& is)
    -> std::expected<FileStatistics, bool> 
{
    auto stats {FileStatistics {}};
    constexpr std::size_t bufsize {262144};
    char buf[bufsize];
    [[maybe_unused]] bool in_word {false};

    std::uintmax_t line_pos {0};
    constexpr std::size_t tab_width {8};

    while (is) {
        is.read(buf, bufsize);
        const auto count {static_cast<std::size_t>(is.gcount())};

        if (count == 0) {
            break;
        }

        stats.bytes += count;

        if (options.count_lines or options.count_max_line_length or
            options.count_words) {
            for (std::size_t i {0}; i < count; ++i) {
                const auto c {static_cast<unsigned char>(buf[i])};

                switch (c) {
                case '\n':
                    ++stats.lines;
                    [[fallthrough]];

                case '\r':
                case '\f':
                    stats.max_line_length =
                        std::max(line_pos, stats.max_line_length);
                    line_pos = 0;
                    in_word = false;
                    break;

                case '\t':
                    line_pos += tab_width - (line_pos % tab_width);
                    in_word = false;
                    break;

                case ' ':
                    ++line_pos;
                    [[fallthrough]];

                case '\v':
                    in_word = false;
                    break;

                default:
                    line_pos += isprint(c) != 0;
                    const bool in_word2 = !isspace(c);
                    stats.words += !in_word & in_word2;
                    in_word = in_word2;
                    break;
                }
            }
        }
    }

    stats.max_line_length = std::max(line_pos, stats.max_line_length);

    if (is.bad()) {
        return std::unexpected {false};
    }
    return stats;
}

[[nodiscard]] static auto wc_file(const Options& options, 
                                  std::istream& is,
                                  const char* file, 
                                  int nfiles,
                                  FileStatistics& total_stats) -> int 
{
    auto const stats {wc(options, is)};

    if (not stats) {
        read_err(std::cerr, file);
        return EXIT_SUCCESS; /* We only want to exit on overflow. */
    }

    write_counts(std::cout, options, stats.value(), file);

    if (nfiles > 1) {
        total_stats.max_line_length =
            std::max(stats->max_line_length, total_stats.max_line_length);

        if (!chkd_add(total_stats.lines, total_stats.lines, stats->lines)) {
            std::cerr << "Error: integer overflow in total lines.\n";
            return EXIT_FAILURE;
        } else if (!chkd_add(total_stats.words, total_stats.words,
                             stats->words)) {
            std::cerr << "Error: integer overflow in total words.\n";
            return EXIT_FAILURE;
        } else if (!chkd_add(total_stats.bytes, total_stats.bytes,
                             stats->bytes)) {
            std::cerr << "Error: integer overflow in total bytes.\n";
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] static auto wc_file(const Options& options,
                                  std::istream& is,
                                  const char* file) -> int 
{
    auto const stats {wc(options, is)};

    if (not stats) {
        read_err(std::cerr, file);
        return EXIT_FAILURE;
    }

    write_counts(std::cout, options, stats.value(), nullptr);

    return EXIT_SUCCESS;
}

auto main(int argc, char* argv[]) -> int 
{
    std::locale::global(std::locale(""));
    std::cin.tie(nullptr);

    auto options {parse_options(argc, argv)};

    if (not options) {
        if (options.error() == ParseOptionsError::help_requested) {
            help(std::cout, argv[0]);
            return EXIT_SUCCESS;
        }

        if (options.error() == ParseOptionsError::unknown_option) {
            usage_err(std::cerr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (not(options->count_lines or options->count_words or
            options->count_bytes or options->count_max_line_length)) {
        options->count_lines = options->count_words = options->count_bytes =
            true;
    }

    if (optind == argc) {
        std::ios_base::sync_with_stdio(false);
        return wc_file(options.value(), std::cin, "stdin");
    }

    auto total_stats {FileStatistics {}};
    int nfiles {optind < argc ? argc - optind : 1};

    for (int i {optind}; i < argc; ++i) {
        auto const path {std::string_view {argv[i]}};
        
        if (fs::is_directory(path)) {
            std::cerr << std::format("wc: {}: Is a directory.\n", argv[i]);
            write_counts(std::cout, options.value(), FileStatistics {0},
                     argv[i]);
        } else if (fs::is_regular_file(path)) {
            auto file {std::ifstream {argv[i], std::ios::binary}};

            if (wc_file(options.value(), file, argv[i], nfiles,
                        total_stats) == EXIT_FAILURE) {
                return EXIT_FAILURE;
            }
        } else if (path == "-") {
            if (wc_file(options.value(), std::cin, argv[i], nfiles,
                        total_stats) == EXIT_FAILURE) {
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << std::format("wc: {}: No such file or directory.\n", 
                     argv[i]);
        }
    }

    if (nfiles > 1) {
        write_counts(std::cout, options.value(), total_stats, "total");
    }
}
