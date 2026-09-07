#include "editor/SyntaxHighlighter.h"

#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace keys::editor {
namespace {

using Language = SyntaxHighlighter::Language;

/// Keywords per language. Deliberately the reserved words only: adding library
/// names would mean maintaining a list that is always out of date and colouring
/// an identifier the user redefined.
const QStringList& keywordsFor(Language language)
{
    static const QStringList none;

    static const QStringList c = {
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("auto"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
        QStringLiteral("class"), QStringLiteral("concept"), QStringLiteral("const"),
        QStringLiteral("consteval"), QStringLiteral("constexpr"), QStringLiteral("constinit"),
        QStringLiteral("continue"), QStringLiteral("co_await"), QStringLiteral("co_return"),
        QStringLiteral("co_yield"), QStringLiteral("default"), QStringLiteral("delete"),
        QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
        QStringLiteral("false"), QStringLiteral("final"), QStringLiteral("for"),
        QStringLiteral("friend"), QStringLiteral("goto"), QStringLiteral("if"),
        QStringLiteral("inline"), QStringLiteral("mutable"), QStringLiteral("namespace"),
        QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("nullptr"),
        QStringLiteral("operator"), QStringLiteral("override"), QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("register"),
        QStringLiteral("requires"), QStringLiteral("return"), QStringLiteral("sizeof"),
        QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("struct"),
        QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
        QStringLiteral("thread_local"), QStringLiteral("throw"), QStringLiteral("true"),
        QStringLiteral("try"), QStringLiteral("typedef"), QStringLiteral("typename"),
        QStringLiteral("union"), QStringLiteral("using"), QStringLiteral("virtual"),
        QStringLiteral("volatile"), QStringLiteral("while"),
    };

    static const QStringList python = {
        QStringLiteral("and"), QStringLiteral("as"), QStringLiteral("assert"),
        QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"), QStringLiteral("elif"), QStringLiteral("else"),
        QStringLiteral("except"), QStringLiteral("False"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("global"),
        QStringLiteral("if"), QStringLiteral("import"), QStringLiteral("in"),
        QStringLiteral("is"), QStringLiteral("lambda"), QStringLiteral("None"),
        QStringLiteral("nonlocal"), QStringLiteral("not"), QStringLiteral("or"),
        QStringLiteral("pass"), QStringLiteral("raise"), QStringLiteral("return"),
        QStringLiteral("True"), QStringLiteral("try"), QStringLiteral("while"),
        QStringLiteral("with"), QStringLiteral("yield"),
    };

    static const QStringList javascript = {
        QStringLiteral("as"), QStringLiteral("async"), QStringLiteral("await"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
        QStringLiteral("class"), QStringLiteral("const"), QStringLiteral("continue"),
        QStringLiteral("debugger"), QStringLiteral("default"), QStringLiteral("delete"),
        QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("export"),
        QStringLiteral("extends"), QStringLiteral("false"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("function"),
        QStringLiteral("if"), QStringLiteral("implements"), QStringLiteral("import"),
        QStringLiteral("in"), QStringLiteral("instanceof"), QStringLiteral("interface"),
        QStringLiteral("let"), QStringLiteral("new"), QStringLiteral("null"),
        QStringLiteral("of"), QStringLiteral("return"), QStringLiteral("static"),
        QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("type"), QStringLiteral("typeof"), QStringLiteral("undefined"),
        QStringLiteral("var"), QStringLiteral("void"), QStringLiteral("while"),
        QStringLiteral("yield"),
    };

    static const QStringList rust = {
        QStringLiteral("as"), QStringLiteral("async"), QStringLiteral("await"),
        QStringLiteral("break"), QStringLiteral("const"), QStringLiteral("continue"),
        QStringLiteral("crate"), QStringLiteral("dyn"), QStringLiteral("else"),
        QStringLiteral("enum"), QStringLiteral("extern"), QStringLiteral("false"),
        QStringLiteral("fn"), QStringLiteral("for"), QStringLiteral("if"),
        QStringLiteral("impl"), QStringLiteral("in"), QStringLiteral("let"),
        QStringLiteral("loop"), QStringLiteral("match"), QStringLiteral("mod"),
        QStringLiteral("move"), QStringLiteral("mut"), QStringLiteral("pub"),
        QStringLiteral("ref"), QStringLiteral("return"), QStringLiteral("self"),
        QStringLiteral("static"), QStringLiteral("struct"), QStringLiteral("super"),
        QStringLiteral("trait"), QStringLiteral("true"), QStringLiteral("type"),
        QStringLiteral("unsafe"), QStringLiteral("use"), QStringLiteral("where"),
        QStringLiteral("while"),
    };

    static const QStringList go = {
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("chan"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"),
        QStringLiteral("defer"), QStringLiteral("else"), QStringLiteral("fallthrough"),
        QStringLiteral("for"), QStringLiteral("func"), QStringLiteral("go"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("import"),
        QStringLiteral("interface"), QStringLiteral("map"), QStringLiteral("package"),
        QStringLiteral("range"), QStringLiteral("return"), QStringLiteral("select"),
        QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("type"),
        QStringLiteral("var"),
    };

    static const QStringList qml = {
        QStringLiteral("as"), QStringLiteral("break"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("const"), QStringLiteral("continue"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("false"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("import"),
        QStringLiteral("in"), QStringLiteral("let"), QStringLiteral("new"),
        QStringLiteral("null"), QStringLiteral("pragma"), QStringLiteral("property"),
        QStringLiteral("readonly"), QStringLiteral("required"), QStringLiteral("return"),
        QStringLiteral("signal"), QStringLiteral("switch"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("typeof"), QStringLiteral("var"), QStringLiteral("while"),
    };

    static const QStringList shell = {
        QStringLiteral("case"), QStringLiteral("do"), QStringLiteral("done"),
        QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("esac"),
        QStringLiteral("export"), QStringLiteral("fi"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("in"),
        QStringLiteral("local"), QStringLiteral("return"), QStringLiteral("then"),
        QStringLiteral("until"), QStringLiteral("while"),
    };


    static const QStringList java = {
        QStringLiteral("abstract"), QStringLiteral("assert"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("extends"),
        QStringLiteral("final"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("implements"),
        QStringLiteral("import"), QStringLiteral("instanceof"), QStringLiteral("interface"),
        QStringLiteral("native"), QStringLiteral("new"), QStringLiteral("package"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("return"), QStringLiteral("static"), QStringLiteral("strictfp"),
        QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("synchronized"),
        QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("throws"),
        QStringLiteral("transient"), QStringLiteral("try"), QStringLiteral("volatile"),
        QStringLiteral("while"), QStringLiteral("var"), QStringLiteral("record"),
        QStringLiteral("sealed"), QStringLiteral("permits"), QStringLiteral("yield"),
    };

    static const QStringList ruby = {
        QStringLiteral("alias"), QStringLiteral("and"), QStringLiteral("begin"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("class"),
        QStringLiteral("def"), QStringLiteral("defined?"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("elsif"), QStringLiteral("end"),
        QStringLiteral("ensure"), QStringLiteral("for"), QStringLiteral("if"),
        QStringLiteral("in"), QStringLiteral("module"), QStringLiteral("next"),
        QStringLiteral("not"), QStringLiteral("or"), QStringLiteral("redo"),
        QStringLiteral("rescue"), QStringLiteral("retry"), QStringLiteral("return"),
        QStringLiteral("self"), QStringLiteral("super"), QStringLiteral("then"),
        QStringLiteral("undef"), QStringLiteral("unless"), QStringLiteral("until"),
        QStringLiteral("when"), QStringLiteral("while"), QStringLiteral("yield"),
        QStringLiteral("require"), QStringLiteral("require_relative"), QStringLiteral("attr_accessor"),
        QStringLiteral("attr_reader"), QStringLiteral("attr_writer"), QStringLiteral("lambda"),
        QStringLiteral("proc"),
    };

    // Written both cases in real code; matched case-insensitively.
    static const QStringList sql = {
        QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"),
        QStringLiteral("insert"), QStringLiteral("update"), QStringLiteral("delete"),
        QStringLiteral("create"), QStringLiteral("drop"), QStringLiteral("alter"),
        QStringLiteral("table"), QStringLiteral("index"), QStringLiteral("view"),
        QStringLiteral("join"), QStringLiteral("inner"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("outer"), QStringLiteral("full"),
        QStringLiteral("on"), QStringLiteral("group"), QStringLiteral("by"),
        QStringLiteral("order"), QStringLiteral("having"), QStringLiteral("limit"),
        QStringLiteral("offset"), QStringLiteral("union"), QStringLiteral("all"),
        QStringLiteral("distinct"), QStringLiteral("as"), QStringLiteral("into"),
        QStringLiteral("values"), QStringLiteral("set"), QStringLiteral("primary"),
        QStringLiteral("key"), QStringLiteral("foreign"), QStringLiteral("references"),
        QStringLiteral("constraint"), QStringLiteral("unique"), QStringLiteral("not"),
        QStringLiteral("null"), QStringLiteral("default"), QStringLiteral("cascade"),
        QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("in"),
        QStringLiteral("exists"), QStringLiteral("between"), QStringLiteral("like"),
        QStringLiteral("is"), QStringLiteral("case"), QStringLiteral("when"),
        QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("end"),
        QStringLiteral("with"), QStringLiteral("returning"),
    };

    // CMake's commands rather than keywords: the language is almost entirely
    // commands, and colouring only if/foreach would leave a build file flat.
    static const QStringList cmake = {
        QStringLiteral("if"), QStringLiteral("elseif"), QStringLiteral("else"),
        QStringLiteral("endif"), QStringLiteral("foreach"), QStringLiteral("endforeach"),
        QStringLiteral("while"), QStringLiteral("endwhile"), QStringLiteral("function"),
        QStringLiteral("endfunction"), QStringLiteral("macro"), QStringLiteral("endmacro"),
        QStringLiteral("return"), QStringLiteral("break"), QStringLiteral("continue"),
        QStringLiteral("set"), QStringLiteral("unset"), QStringLiteral("list"),
        QStringLiteral("string"), QStringLiteral("file"), QStringLiteral("find_package"),
        QStringLiteral("include"), QStringLiteral("add_subdirectory"), QStringLiteral("add_executable"),
        QStringLiteral("add_library"), QStringLiteral("target_link_libraries"), QStringLiteral("target_include_directories"),
        QStringLiteral("target_compile_definitions"), QStringLiteral("target_compile_features"), QStringLiteral("target_compile_options"),
        QStringLiteral("install"), QStringLiteral("option"), QStringLiteral("project"),
        QStringLiteral("cmake_minimum_required"), QStringLiteral("message"), QStringLiteral("get_target_property"),
        QStringLiteral("set_target_properties"), QStringLiteral("add_custom_command"), QStringLiteral("add_custom_target"),
        QStringLiteral("add_test"), QStringLiteral("enable_testing"), QStringLiteral("configure_file"),
    };

    // YAML has no keywords; these are the literals worth marking as values.
    static const QStringList yaml = {
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("on"),
        QStringLiteral("off"),
    };

    static const QStringList toml = {
        QStringLiteral("true"), QStringLiteral("false"),
    };

    // TypeScript is JavaScript plus type-level words. Built from the JS list so
    // the two cannot drift apart.
    static const QStringList typescript = [] {
        QStringList words = javascript;
        words << QStringList{
            QStringLiteral("abstract"), QStringLiteral("any"), QStringLiteral("as"),
            QStringLiteral("asserts"), QStringLiteral("bigint"), QStringLiteral("boolean"),
            QStringLiteral("declare"), QStringLiteral("enum"), QStringLiteral("implements"),
            QStringLiteral("infer"), QStringLiteral("is"), QStringLiteral("keyof"),
            QStringLiteral("namespace"), QStringLiteral("never"), QStringLiteral("number"),
            QStringLiteral("object"), QStringLiteral("override"), QStringLiteral("private"),
            QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("readonly"),
            QStringLiteral("require"), QStringLiteral("satisfies"), QStringLiteral("string"),
            QStringLiteral("symbol"), QStringLiteral("type"), QStringLiteral("undefined"),
            QStringLiteral("unique"), QStringLiteral("unknown"),
        };
        words.sort();
        return words;
    }();

    /// C#. Contextual keywords (`async`, `record`, `when`) are included:
    /// they read as keywords everywhere a reader meets them, and the
    /// alternative is a parser rather than a highlighter.
    static const QStringList csharp = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("as"), QStringLiteral("base"),
            QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
            QStringLiteral("checked"), QStringLiteral("class"),
            QStringLiteral("const"), QStringLiteral("continue"),
            QStringLiteral("default"), QStringLiteral("delegate"),
            QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("enum"),
            QStringLiteral("event"), QStringLiteral("explicit"),
            QStringLiteral("extern"), QStringLiteral("finally"),
            QStringLiteral("fixed"), QStringLiteral("for"),
            QStringLiteral("foreach"), QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("implicit"), QStringLiteral("in"),
            QStringLiteral("interface"), QStringLiteral("internal"),
            QStringLiteral("is"), QStringLiteral("lock"),
            QStringLiteral("namespace"), QStringLiteral("new"),
            QStringLiteral("operator"), QStringLiteral("out"),
            QStringLiteral("override"), QStringLiteral("params"),
            QStringLiteral("private"), QStringLiteral("protected"),
            QStringLiteral("public"), QStringLiteral("readonly"),
            QStringLiteral("ref"), QStringLiteral("return"),
            QStringLiteral("sealed"), QStringLiteral("sizeof"),
            QStringLiteral("stackalloc"), QStringLiteral("static"),
            QStringLiteral("struct"), QStringLiteral("switch"),
            QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("try"),
            QStringLiteral("typeof"), QStringLiteral("unchecked"),
            QStringLiteral("unsafe"), QStringLiteral("using"),
            QStringLiteral("virtual"), QStringLiteral("volatile"),
            QStringLiteral("while"), QStringLiteral("add"), QStringLiteral("and"),
            QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("get"),
            QStringLiteral("global"), QStringLiteral("init"),
            QStringLiteral("nameof"), QStringLiteral("not"), QStringLiteral("or"),
            QStringLiteral("partial"), QStringLiteral("record"),
            QStringLiteral("remove"), QStringLiteral("required"),
            QStringLiteral("set"), QStringLiteral("value"), QStringLiteral("when"),
            QStringLiteral("where"), QStringLiteral("with"), QStringLiteral("yield"),
        };
        words.sort();
        return words;
    }();

    /// Swift, including the concurrency and ownership keywords.
    static const QStringList swift = [] {
        QStringList words = {
            QStringLiteral("associatedtype"), QStringLiteral("await"),
            QStringLiteral("borrowing"), QStringLiteral("break"),
            QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
            QStringLiteral("consuming"), QStringLiteral("continue"),
            QStringLiteral("convenience"), QStringLiteral("default"),
            QStringLiteral("defer"), QStringLiteral("deinit"),
            QStringLiteral("didSet"), QStringLiteral("distributed"),
            QStringLiteral("do"), QStringLiteral("dynamic"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("extension"),
            QStringLiteral("fallthrough"), QStringLiteral("fileprivate"),
            QStringLiteral("final"), QStringLiteral("for"), QStringLiteral("func"),
            QStringLiteral("get"), QStringLiteral("guard"), QStringLiteral("if"),
            QStringLiteral("import"), QStringLiteral("in"),
            QStringLiteral("indirect"), QStringLiteral("infix"),
            QStringLiteral("init"), QStringLiteral("inout"),
            QStringLiteral("internal"), QStringLiteral("is"), QStringLiteral("lazy"),
            QStringLiteral("let"), QStringLiteral("mutating"),
            QStringLiteral("nonisolated"), QStringLiteral("nonmutating"),
            QStringLiteral("open"), QStringLiteral("operator"),
            QStringLiteral("optional"), QStringLiteral("override"),
            QStringLiteral("postfix"), QStringLiteral("precedencegroup"),
            QStringLiteral("prefix"), QStringLiteral("private"),
            QStringLiteral("protocol"), QStringLiteral("public"),
            QStringLiteral("repeat"), QStringLiteral("required"),
            QStringLiteral("rethrows"), QStringLiteral("return"),
            QStringLiteral("safe"), QStringLiteral("self"), QStringLiteral("set"),
            QStringLiteral("some"), QStringLiteral("static"),
            QStringLiteral("struct"), QStringLiteral("subscript"),
            QStringLiteral("super"), QStringLiteral("switch"),
            QStringLiteral("throw"), QStringLiteral("throws"), QStringLiteral("try"),
            QStringLiteral("typealias"), QStringLiteral("unowned"),
            QStringLiteral("unsafe"), QStringLiteral("var"), QStringLiteral("weak"),
            QStringLiteral("where"), QStringLiteral("while"),
            QStringLiteral("willSet"), QStringLiteral("actor"),
            QStringLiteral("async"),
        };
        words.sort();
        return words;
    }();

    /// PHP. The keywords only - the `<?php` tags and the HTML around
    /// them are handled separately.
    static const QStringList php = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("and"),
            QStringLiteral("array"), QStringLiteral("as"), QStringLiteral("break"),
            QStringLiteral("callable"), QStringLiteral("case"),
            QStringLiteral("catch"), QStringLiteral("class"),
            QStringLiteral("clone"), QStringLiteral("const"),
            QStringLiteral("continue"), QStringLiteral("declare"),
            QStringLiteral("default"), QStringLiteral("do"), QStringLiteral("echo"),
            QStringLiteral("else"), QStringLiteral("elseif"),
            QStringLiteral("empty"), QStringLiteral("enddeclare"),
            QStringLiteral("endfor"), QStringLiteral("endforeach"),
            QStringLiteral("endif"), QStringLiteral("endswitch"),
            QStringLiteral("endwhile"), QStringLiteral("enum"),
            QStringLiteral("extends"), QStringLiteral("final"),
            QStringLiteral("finally"), QStringLiteral("fn"), QStringLiteral("for"),
            QStringLiteral("foreach"), QStringLiteral("function"),
            QStringLiteral("global"), QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("implements"), QStringLiteral("include"),
            QStringLiteral("include_once"), QStringLiteral("instanceof"),
            QStringLiteral("insteadof"), QStringLiteral("interface"),
            QStringLiteral("isset"), QStringLiteral("list"), QStringLiteral("match"),
            QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("or"),
            QStringLiteral("print"), QStringLiteral("private"),
            QStringLiteral("protected"), QStringLiteral("public"),
            QStringLiteral("readonly"), QStringLiteral("require"),
            QStringLiteral("require_once"), QStringLiteral("return"),
            QStringLiteral("static"), QStringLiteral("switch"),
            QStringLiteral("throw"), QStringLiteral("trait"), QStringLiteral("try"),
            QStringLiteral("unset"), QStringLiteral("use"), QStringLiteral("var"),
            QStringLiteral("while"), QStringLiteral("xor"), QStringLiteral("yield"),
        };
        words.sort();
        return words;
    }();

    /// Kotlin, including the soft keywords and annotation-use-site targets.
    static const QStringList kotlin = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("actual"),
            QStringLiteral("annotation"), QStringLiteral("as"),
            QStringLiteral("break"), QStringLiteral("by"), QStringLiteral("catch"),
            QStringLiteral("class"), QStringLiteral("companion"),
            QStringLiteral("const"), QStringLiteral("constructor"),
            QStringLiteral("continue"), QStringLiteral("crossinline"),
            QStringLiteral("data"), QStringLiteral("delegate"), QStringLiteral("do"),
            QStringLiteral("dynamic"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("expect"),
            QStringLiteral("external"), QStringLiteral("field"),
            QStringLiteral("file"), QStringLiteral("final"),
            QStringLiteral("finally"), QStringLiteral("for"), QStringLiteral("fun"),
            QStringLiteral("get"), QStringLiteral("if"), QStringLiteral("import"),
            QStringLiteral("in"), QStringLiteral("infix"), QStringLiteral("init"),
            QStringLiteral("inline"), QStringLiteral("inner"),
            QStringLiteral("interface"), QStringLiteral("internal"),
            QStringLiteral("is"), QStringLiteral("lateinit"),
            QStringLiteral("noinline"), QStringLiteral("object"),
            QStringLiteral("open"), QStringLiteral("operator"),
            QStringLiteral("out"), QStringLiteral("override"),
            QStringLiteral("package"), QStringLiteral("param"),
            QStringLiteral("private"), QStringLiteral("property"),
            QStringLiteral("protected"), QStringLiteral("public"),
            QStringLiteral("receiver"), QStringLiteral("reified"),
            QStringLiteral("return"), QStringLiteral("sealed"),
            QStringLiteral("set"), QStringLiteral("setparam"),
            QStringLiteral("super"), QStringLiteral("suspend"),
            QStringLiteral("tailrec"), QStringLiteral("this"),
            QStringLiteral("throw"), QStringLiteral("try"),
            QStringLiteral("typealias"), QStringLiteral("typeof"),
            QStringLiteral("val"), QStringLiteral("value"), QStringLiteral("var"),
            QStringLiteral("vararg"), QStringLiteral("when"),
            QStringLiteral("where"), QStringLiteral("while"),
        };
        words.sort();
        return words;
    }();

    /// Dart, including the null-safety and macro keywords.
    static const QStringList dart = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("as"),
            QStringLiteral("assert"), QStringLiteral("async"),
            QStringLiteral("await"), QStringLiteral("base"), QStringLiteral("break"),
            QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
            QStringLiteral("const"), QStringLiteral("continue"),
            QStringLiteral("covariant"), QStringLiteral("default"),
            QStringLiteral("deferred"), QStringLiteral("do"),
            QStringLiteral("dynamic"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("export"),
            QStringLiteral("extends"), QStringLiteral("extension"),
            QStringLiteral("external"), QStringLiteral("factory"),
            QStringLiteral("false"), QStringLiteral("final"),
            QStringLiteral("finally"), QStringLiteral("for"), QStringLiteral("get"),
            QStringLiteral("hide"), QStringLiteral("if"),
            QStringLiteral("implements"), QStringLiteral("import"),
            QStringLiteral("in"), QStringLiteral("interface"), QStringLiteral("is"),
            QStringLiteral("late"), QStringLiteral("library"),
            QStringLiteral("mixin"), QStringLiteral("new"), QStringLiteral("null"),
            QStringLiteral("on"), QStringLiteral("operator"), QStringLiteral("part"),
            QStringLiteral("required"), QStringLiteral("rethrow"),
            QStringLiteral("return"), QStringLiteral("sealed"),
            QStringLiteral("set"), QStringLiteral("show"), QStringLiteral("static"),
            QStringLiteral("super"), QStringLiteral("switch"),
            QStringLiteral("sync"), QStringLiteral("this"), QStringLiteral("throw"),
            QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
            QStringLiteral("var"), QStringLiteral("void"), QStringLiteral("when"),
            QStringLiteral("while"), QStringLiteral("with"), QStringLiteral("yield"),
        };
        words.sort();
        return words;
    }();

    /// Scala, including the Scala 3 additions.
    static const QStringList scala = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("case"),
            QStringLiteral("catch"), QStringLiteral("class"), QStringLiteral("def"),
            QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("enum"),
            QStringLiteral("export"), QStringLiteral("extends"),
            QStringLiteral("false"), QStringLiteral("final"),
            QStringLiteral("finally"), QStringLiteral("for"),
            QStringLiteral("forSome"), QStringLiteral("given"), QStringLiteral("if"),
            QStringLiteral("implicit"), QStringLiteral("import"),
            QStringLiteral("lazy"), QStringLiteral("match"), QStringLiteral("new"),
            QStringLiteral("null"), QStringLiteral("object"),
            QStringLiteral("override"), QStringLiteral("package"),
            QStringLiteral("private"), QStringLiteral("protected"),
            QStringLiteral("return"), QStringLiteral("sealed"),
            QStringLiteral("super"), QStringLiteral("then"), QStringLiteral("this"),
            QStringLiteral("throw"), QStringLiteral("trait"), QStringLiteral("true"),
            QStringLiteral("try"), QStringLiteral("type"), QStringLiteral("using"),
            QStringLiteral("val"), QStringLiteral("var"), QStringLiteral("while"),
            QStringLiteral("with"), QStringLiteral("yield"),
        };
        words.sort();
        return words;
    }();

    /// Lua. A small language: this is nearly all of it.
    static const QStringList lua = [] {
        QStringList words = {
            QStringLiteral("and"), QStringLiteral("break"), QStringLiteral("do"),
            QStringLiteral("else"), QStringLiteral("elseif"), QStringLiteral("end"),
            QStringLiteral("false"), QStringLiteral("for"),
            QStringLiteral("function"), QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("in"), QStringLiteral("local"), QStringLiteral("nil"),
            QStringLiteral("not"), QStringLiteral("or"), QStringLiteral("repeat"),
            QStringLiteral("return"), QStringLiteral("then"), QStringLiteral("true"),
            QStringLiteral("until"), QStringLiteral("while"),
        };
        words.sort();
        return words;
    }();

    /// Perl, including the common named operators.
    static const QStringList perl = [] {
        QStringList words = {
            QStringLiteral("and"), QStringLiteral("cmp"), QStringLiteral("continue"),
            QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("elsif"),
            QStringLiteral("eq"), QStringLiteral("eval"), QStringLiteral("exit"),
            QStringLiteral("for"), QStringLiteral("foreach"), QStringLiteral("ge"),
            QStringLiteral("given"), QStringLiteral("goto"), QStringLiteral("gt"),
            QStringLiteral("if"), QStringLiteral("last"), QStringLiteral("le"),
            QStringLiteral("local"), QStringLiteral("lt"), QStringLiteral("my"),
            QStringLiteral("ne"), QStringLiteral("next"), QStringLiteral("no"),
            QStringLiteral("not"), QStringLiteral("or"), QStringLiteral("our"),
            QStringLiteral("package"), QStringLiteral("redo"), QStringLiteral("ref"),
            QStringLiteral("require"), QStringLiteral("return"),
            QStringLiteral("say"), QStringLiteral("sub"), QStringLiteral("switch"),
            QStringLiteral("tie"), QStringLiteral("unless"), QStringLiteral("untie"),
            QStringLiteral("until"), QStringLiteral("use"),
            QStringLiteral("wantarray"), QStringLiteral("when"),
            QStringLiteral("while"), QStringLiteral("xor"),
        };
        words.sort();
        return words;
    }();

    /// R. Its reserved words are few, so the literals are included -
    /// `TRUE` and `NA` are what a reader scans an R script for.
    static const QStringList rlang = [] {
        QStringList words = {
            QStringLiteral("break"), QStringLiteral("else"), QStringLiteral("for"),
            QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("in"),
            QStringLiteral("next"), QStringLiteral("repeat"),
            QStringLiteral("return"), QStringLiteral("while"),
            QStringLiteral("TRUE"), QStringLiteral("FALSE"), QStringLiteral("NULL"),
            QStringLiteral("NA"), QStringLiteral("Inf"), QStringLiteral("NaN"),
            QStringLiteral("NA_integer_"), QStringLiteral("NA_real_"),
            QStringLiteral("NA_character_"),
        };
        words.sort();
        return words;
    }();

    /// Haskell, including the common language-extension keywords.
    static const QStringList haskell = [] {
        QStringList words = {
            QStringLiteral("case"), QStringLiteral("class"),
            QStringLiteral("data"), QStringLiteral("default"),
            QStringLiteral("deriving"), QStringLiteral("do"),
            QStringLiteral("else"), QStringLiteral("family"),
            QStringLiteral("forall"), QStringLiteral("foreign"),
            QStringLiteral("hiding"), QStringLiteral("if"),
            QStringLiteral("import"), QStringLiteral("in"),
            QStringLiteral("infix"), QStringLiteral("infixl"),
            QStringLiteral("infixr"), QStringLiteral("instance"),
            QStringLiteral("let"), QStringLiteral("mdo"),
            QStringLiteral("module"), QStringLiteral("newtype"),
            QStringLiteral("of"), QStringLiteral("pattern"),
            QStringLiteral("proc"), QStringLiteral("qualified"),
            QStringLiteral("rec"), QStringLiteral("then"),
            QStringLiteral("type"), QStringLiteral("where"),
        };
        words.sort();
        return words;
    }();

    /// Elixir, including the `def` family and the block keywords.
    static const QStringList elixir = [] {
        QStringList words = {
            QStringLiteral("after"), QStringLiteral("alias"),
            QStringLiteral("and"), QStringLiteral("case"),
            QStringLiteral("catch"), QStringLiteral("cond"),
            QStringLiteral("def"), QStringLiteral("defexception"),
            QStringLiteral("defguard"), QStringLiteral("defimpl"),
            QStringLiteral("defmacro"), QStringLiteral("defmodule"),
            QStringLiteral("defoverridable"), QStringLiteral("defp"),
            QStringLiteral("defprotocol"), QStringLiteral("defstruct"),
            QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("end"),
            QStringLiteral("fn"), QStringLiteral("for"), QStringLiteral("if"),
            QStringLiteral("import"), QStringLiteral("in"),
            QStringLiteral("not"), QStringLiteral("or"), QStringLiteral("quote"),
            QStringLiteral("raise"), QStringLiteral("receive"),
            QStringLiteral("require"), QStringLiteral("rescue"),
            QStringLiteral("then"), QStringLiteral("try"),
            QStringLiteral("unless"), QStringLiteral("unquote"),
            QStringLiteral("use"), QStringLiteral("when"),
            QStringLiteral("with"),
        };
        words.sort();
        return words;
    }();

    /// OCaml, including the operator keywords (`land`, `lsl`).
    static const QStringList ocaml = [] {
        QStringList words = {
            QStringLiteral("and"), QStringLiteral("as"),
            QStringLiteral("assert"), QStringLiteral("asr"),
            QStringLiteral("begin"), QStringLiteral("class"),
            QStringLiteral("constraint"), QStringLiteral("do"),
            QStringLiteral("done"), QStringLiteral("downto"),
            QStringLiteral("else"), QStringLiteral("end"),
            QStringLiteral("exception"), QStringLiteral("external"),
            QStringLiteral("for"), QStringLiteral("fun"),
            QStringLiteral("function"), QStringLiteral("functor"),
            QStringLiteral("if"), QStringLiteral("in"),
            QStringLiteral("include"), QStringLiteral("inherit"),
            QStringLiteral("initializer"), QStringLiteral("land"),
            QStringLiteral("lazy"), QStringLiteral("let"), QStringLiteral("lor"),
            QStringLiteral("lsl"), QStringLiteral("lsr"), QStringLiteral("lxor"),
            QStringLiteral("match"), QStringLiteral("method"),
            QStringLiteral("mod"), QStringLiteral("module"),
            QStringLiteral("mutable"), QStringLiteral("new"),
            QStringLiteral("nonrec"), QStringLiteral("object"),
            QStringLiteral("of"), QStringLiteral("open"), QStringLiteral("or"),
            QStringLiteral("private"), QStringLiteral("rec"),
            QStringLiteral("sig"), QStringLiteral("struct"),
            QStringLiteral("then"), QStringLiteral("to"), QStringLiteral("try"),
            QStringLiteral("type"), QStringLiteral("val"),
            QStringLiteral("virtual"), QStringLiteral("when"),
            QStringLiteral("while"), QStringLiteral("with"),
        };
        words.sort();
        return words;
    }();

    /// F#, which shares much of OCaml plus the .NET keywords.
    static const QStringList fsharp = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("and"),
            QStringLiteral("as"), QStringLiteral("assert"),
            QStringLiteral("base"), QStringLiteral("begin"),
            QStringLiteral("class"), QStringLiteral("default"),
            QStringLiteral("delegate"), QStringLiteral("do"),
            QStringLiteral("done"), QStringLiteral("downcast"),
            QStringLiteral("downto"), QStringLiteral("elif"),
            QStringLiteral("else"), QStringLiteral("end"),
            QStringLiteral("exception"), QStringLiteral("extern"),
            QStringLiteral("finally"), QStringLiteral("fixed"),
            QStringLiteral("for"), QStringLiteral("fun"),
            QStringLiteral("function"), QStringLiteral("global"),
            QStringLiteral("if"), QStringLiteral("in"),
            QStringLiteral("inherit"), QStringLiteral("inline"),
            QStringLiteral("interface"), QStringLiteral("internal"),
            QStringLiteral("lazy"), QStringLiteral("let"),
            QStringLiteral("match"), QStringLiteral("member"),
            QStringLiteral("module"), QStringLiteral("mutable"),
            QStringLiteral("namespace"), QStringLiteral("new"),
            QStringLiteral("not"), QStringLiteral("null"), QStringLiteral("of"),
            QStringLiteral("open"), QStringLiteral("or"),
            QStringLiteral("override"), QStringLiteral("private"),
            QStringLiteral("public"), QStringLiteral("rec"),
            QStringLiteral("return"), QStringLiteral("select"),
            QStringLiteral("static"), QStringLiteral("struct"),
            QStringLiteral("then"), QStringLiteral("to"), QStringLiteral("try"),
            QStringLiteral("type"), QStringLiteral("upcast"),
            QStringLiteral("use"), QStringLiteral("val"), QStringLiteral("void"),
            QStringLiteral("when"), QStringLiteral("while"),
            QStringLiteral("with"), QStringLiteral("yield"),
        };
        words.sort();
        return words;
    }();

    /// Zig, including `comptime` and the error-handling keywords.
    static const QStringList zig = [] {
        QStringList words = {
            QStringLiteral("align"), QStringLiteral("allowzero"),
            QStringLiteral("and"), QStringLiteral("anyframe"),
            QStringLiteral("anytype"), QStringLiteral("asm"),
            QStringLiteral("async"), QStringLiteral("await"),
            QStringLiteral("break"), QStringLiteral("callconv"),
            QStringLiteral("catch"), QStringLiteral("comptime"),
            QStringLiteral("const"), QStringLiteral("continue"),
            QStringLiteral("defer"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("errdefer"),
            QStringLiteral("error"), QStringLiteral("export"),
            QStringLiteral("extern"), QStringLiteral("fn"),
            QStringLiteral("for"), QStringLiteral("if"),
            QStringLiteral("inline"), QStringLiteral("noalias"),
            QStringLiteral("noinline"), QStringLiteral("nosuspend"),
            QStringLiteral("opaque"), QStringLiteral("or"),
            QStringLiteral("orelse"), QStringLiteral("packed"),
            QStringLiteral("pub"), QStringLiteral("resume"),
            QStringLiteral("return"), QStringLiteral("linksection"),
            QStringLiteral("struct"), QStringLiteral("suspend"),
            QStringLiteral("switch"), QStringLiteral("test"),
            QStringLiteral("threadlocal"), QStringLiteral("try"),
            QStringLiteral("union"), QStringLiteral("unreachable"),
            QStringLiteral("usingnamespace"), QStringLiteral("var"),
            QStringLiteral("volatile"), QStringLiteral("while"),
        };
        words.sort();
        return words;
    }();

    /// Nim, including the operator keywords (`div`, `shl`, `notin`).
    static const QStringList nim = [] {
        QStringList words = {
            QStringLiteral("addr"), QStringLiteral("and"), QStringLiteral("as"),
            QStringLiteral("asm"), QStringLiteral("bind"),
            QStringLiteral("block"), QStringLiteral("break"),
            QStringLiteral("case"), QStringLiteral("cast"),
            QStringLiteral("concept"), QStringLiteral("const"),
            QStringLiteral("continue"), QStringLiteral("converter"),
            QStringLiteral("defer"), QStringLiteral("discard"),
            QStringLiteral("distinct"), QStringLiteral("div"),
            QStringLiteral("do"), QStringLiteral("elif"), QStringLiteral("else"),
            QStringLiteral("end"), QStringLiteral("enum"),
            QStringLiteral("except"), QStringLiteral("export"),
            QStringLiteral("finally"), QStringLiteral("for"),
            QStringLiteral("from"), QStringLiteral("func"), QStringLiteral("if"),
            QStringLiteral("import"), QStringLiteral("in"),
            QStringLiteral("include"), QStringLiteral("interface"),
            QStringLiteral("is"), QStringLiteral("isnot"),
            QStringLiteral("iterator"), QStringLiteral("let"),
            QStringLiteral("macro"), QStringLiteral("method"),
            QStringLiteral("mixin"), QStringLiteral("mod"),
            QStringLiteral("nil"), QStringLiteral("not"),
            QStringLiteral("notin"), QStringLiteral("object"),
            QStringLiteral("of"), QStringLiteral("or"), QStringLiteral("out"),
            QStringLiteral("proc"), QStringLiteral("ptr"),
            QStringLiteral("raise"), QStringLiteral("ref"),
            QStringLiteral("return"), QStringLiteral("shl"),
            QStringLiteral("shr"), QStringLiteral("static"),
            QStringLiteral("template"), QStringLiteral("try"),
            QStringLiteral("tuple"), QStringLiteral("type"),
            QStringLiteral("using"), QStringLiteral("var"),
            QStringLiteral("when"), QStringLiteral("while"),
            QStringLiteral("xor"), QStringLiteral("yield"),
        };
        words.sort();
        return words;
    }();

    /// Groovy: Java plus `def`, `trait` and `as`.
    static const QStringList groovy = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("as"),
            QStringLiteral("assert"), QStringLiteral("break"),
            QStringLiteral("case"), QStringLiteral("catch"),
            QStringLiteral("class"), QStringLiteral("const"),
            QStringLiteral("continue"), QStringLiteral("def"),
            QStringLiteral("default"), QStringLiteral("do"),
            QStringLiteral("else"), QStringLiteral("enum"),
            QStringLiteral("extends"), QStringLiteral("final"),
            QStringLiteral("finally"), QStringLiteral("for"),
            QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("implements"), QStringLiteral("import"),
            QStringLiteral("in"), QStringLiteral("instanceof"),
            QStringLiteral("interface"), QStringLiteral("new"),
            QStringLiteral("package"), QStringLiteral("return"),
            QStringLiteral("static"), QStringLiteral("super"),
            QStringLiteral("switch"), QStringLiteral("synchronized"),
            QStringLiteral("this"), QStringLiteral("throw"),
            QStringLiteral("throws"), QStringLiteral("trait"),
            QStringLiteral("try"), QStringLiteral("var"),
            QStringLiteral("while"),
        };
        words.sort();
        return words;
    }();

    /// Julia, including the block-ending keywords.
    static const QStringList julia = [] {
        QStringList words = {
            QStringLiteral("abstract"), QStringLiteral("baremodule"),
            QStringLiteral("begin"), QStringLiteral("break"),
            QStringLiteral("catch"), QStringLiteral("const"),
            QStringLiteral("continue"), QStringLiteral("do"),
            QStringLiteral("else"), QStringLiteral("elseif"),
            QStringLiteral("end"), QStringLiteral("export"),
            QStringLiteral("false"), QStringLiteral("finally"),
            QStringLiteral("for"), QStringLiteral("function"),
            QStringLiteral("global"), QStringLiteral("if"),
            QStringLiteral("import"), QStringLiteral("in"),
            QStringLiteral("isa"), QStringLiteral("let"),
            QStringLiteral("local"), QStringLiteral("macro"),
            QStringLiteral("module"), QStringLiteral("mutable"),
            QStringLiteral("outer"), QStringLiteral("primitive"),
            QStringLiteral("quote"), QStringLiteral("return"),
            QStringLiteral("struct"), QStringLiteral("true"),
            QStringLiteral("try"), QStringLiteral("type"),
            QStringLiteral("using"), QStringLiteral("where"),
            QStringLiteral("while"),
        };
        words.sort();
        return words;
    }();

    /// Objective-C: C's keywords plus the @-directives and the
    /// object-model literals a reader actually scans for.
    static const QStringList objectivec = [] {
        QStringList words = {
            QStringLiteral("@autoreleasepool"), QStringLiteral("@catch"),
            QStringLiteral("@class"), QStringLiteral("@dynamic"),
            QStringLiteral("@encode"), QStringLiteral("@end"),
            QStringLiteral("@finally"), QStringLiteral("@implementation"),
            QStringLiteral("@import"), QStringLiteral("@interface"),
            QStringLiteral("@optional"), QStringLiteral("@package"),
            QStringLiteral("@private"), QStringLiteral("@property"),
            QStringLiteral("@protocol"), QStringLiteral("@protected"),
            QStringLiteral("@public"), QStringLiteral("@required"),
            QStringLiteral("@selector"), QStringLiteral("@synchronized"),
            QStringLiteral("@synthesize"), QStringLiteral("@throw"),
            QStringLiteral("@try"), QStringLiteral("auto"),
            QStringLiteral("break"), QStringLiteral("case"),
            QStringLiteral("char"), QStringLiteral("const"),
            QStringLiteral("continue"), QStringLiteral("default"),
            QStringLiteral("do"), QStringLiteral("double"),
            QStringLiteral("else"), QStringLiteral("enum"),
            QStringLiteral("extern"), QStringLiteral("float"),
            QStringLiteral("for"), QStringLiteral("goto"), QStringLiteral("if"),
            QStringLiteral("in"), QStringLiteral("inline"),
            QStringLiteral("instancetype"), QStringLiteral("int"),
            QStringLiteral("long"), QStringLiteral("nil"), QStringLiteral("NO"),
            QStringLiteral("out"), QStringLiteral("register"),
            QStringLiteral("return"), QStringLiteral("short"),
            QStringLiteral("signed"), QStringLiteral("sizeof"),
            QStringLiteral("static"), QStringLiteral("struct"),
            QStringLiteral("switch"), QStringLiteral("typedef"),
            QStringLiteral("union"), QStringLiteral("unsigned"),
            QStringLiteral("void"), QStringLiteral("volatile"),
            QStringLiteral("while"), QStringLiteral("YES"), QStringLiteral("id"),
            QStringLiteral("BOOL"), QStringLiteral("SEL"), QStringLiteral("IMP"),
            QStringLiteral("Class"),
        };
        words.sort();
        return words;
    }();

    /// Assembly mnemonics and directives, for x86 and ARM.
    ///
    /// Deliberately not exhaustive, and it cannot be: the vocabulary is
    /// per-architecture and grows with every instruction-set extension.
    /// These are the instructions and directives that appear in almost
    /// any listing, which is what makes the structure readable - the
    /// rest is carried by labels, registers and comments, which are
    /// recognised by shape rather than by name.
    static const QStringList assembly = [] {
        QStringList words = {
            QStringLiteral("mov"), QStringLiteral("movl"),
            QStringLiteral("movq"), QStringLiteral("movw"),
            QStringLiteral("movb"), QStringLiteral("lea"),
            QStringLiteral("push"), QStringLiteral("pop"), QStringLiteral("add"),
            QStringLiteral("sub"), QStringLiteral("mul"), QStringLiteral("imul"),
            QStringLiteral("div"), QStringLiteral("idiv"), QStringLiteral("inc"),
            QStringLiteral("dec"), QStringLiteral("neg"), QStringLiteral("cmp"),
            QStringLiteral("test"), QStringLiteral("and"), QStringLiteral("or"),
            QStringLiteral("xor"), QStringLiteral("not"), QStringLiteral("shl"),
            QStringLiteral("shr"), QStringLiteral("sal"), QStringLiteral("sar"),
            QStringLiteral("rol"), QStringLiteral("ror"), QStringLiteral("jmp"),
            QStringLiteral("je"), QStringLiteral("jne"), QStringLiteral("jz"),
            QStringLiteral("jnz"), QStringLiteral("jg"), QStringLiteral("jge"),
            QStringLiteral("jl"), QStringLiteral("jle"), QStringLiteral("ja"),
            QStringLiteral("jae"), QStringLiteral("jb"), QStringLiteral("jbe"),
            QStringLiteral("call"), QStringLiteral("ret"),
            QStringLiteral("leave"), QStringLiteral("enter"),
            QStringLiteral("nop"), QStringLiteral("hlt"), QStringLiteral("int"),
            QStringLiteral("syscall"), QStringLiteral("sysret"),
            QStringLiteral("loop"), QStringLiteral("movzx"),
            QStringLiteral("movsx"), QStringLiteral("cbw"),
            QStringLiteral("cwd"), QStringLiteral("cdq"), QStringLiteral("xchg"),
            QStringLiteral("adc"), QStringLiteral("sbb"), QStringLiteral("cmov"),
            QStringLiteral("sete"), QStringLiteral("setne"),
            QStringLiteral("setg"), QStringLiteral("setl"),
            QStringLiteral("fld"), QStringLiteral("fst"), QStringLiteral("fadd"),
            QStringLiteral("fsub"), QStringLiteral("fmul"),
            QStringLiteral("fdiv"), QStringLiteral("ldr"), QStringLiteral("str"),
            QStringLiteral("ldp"), QStringLiteral("stp"), QStringLiteral("adr"),
            QStringLiteral("adrp"), QStringLiteral("bl"), QStringLiteral("blr"),
            QStringLiteral("br"), QStringLiteral("cbz"), QStringLiteral("cbnz"),
            QStringLiteral("tbz"), QStringLiteral("tbnz"),
            QStringLiteral("adds"), QStringLiteral("subs"),
            QStringLiteral("muls"), QStringLiteral("mvn"), QStringLiteral("orr"),
            QStringLiteral("eor"), QStringLiteral("bic"), QStringLiteral("lsl"),
            QStringLiteral("lsr"), QStringLiteral("asr"), QStringLiteral("b"),
            QStringLiteral("beq"), QStringLiteral("bne"), QStringLiteral("bgt"),
            QStringLiteral("blt"), QStringLiteral("bge"), QStringLiteral("ble"),
            QStringLiteral("bx"), QStringLiteral("vmov"), QStringLiteral("vadd"),
            QStringLiteral("vsub"), QStringLiteral("vmul"),
            QStringLiteral("vldr"), QStringLiteral("vstr"),
            QStringLiteral("section"), QStringLiteral("global"),
            QStringLiteral("extern"), QStringLiteral("db"), QStringLiteral("dw"),
            QStringLiteral("dd"), QStringLiteral("dq"), QStringLiteral("resb"),
            QStringLiteral("resw"), QStringLiteral("resd"),
            QStringLiteral("resq"), QStringLiteral("equ"),
            QStringLiteral("times"), QStringLiteral("align"),
            QStringLiteral("byte"), QStringLiteral("word"),
            QStringLiteral("dword"), QStringLiteral("qword"),
            QStringLiteral("ptr"), QStringLiteral("offset"),
            QStringLiteral("short"), QStringLiteral("near"),
            QStringLiteral("far"), QStringLiteral("proc"),
            QStringLiteral("endp"), QStringLiteral("macro"),
            QStringLiteral("endm"), QStringLiteral("include"),
            QStringLiteral("incbin"), QStringLiteral("org"),
            QStringLiteral("bits"), QStringLiteral("default"),
            QStringLiteral("rep"), QStringLiteral("repe"),
            QStringLiteral("repne"),
        };
        words.sort();
        return words;
    }();

    switch (language) {
    case Language::C:          return c;
    case Language::Python:     return python;
    case Language::JavaScript: return javascript;
    case Language::Rust:       return rust;
    case Language::Go:         return go;
    case Language::Qml:        return qml;
    case Language::Shell:      return shell;
    case Language::TypeScript: return typescript;
    case Language::Java:       return java;
    case Language::Ruby:       return ruby;
    case Language::Sql:        return sql;
    case Language::CMake:      return cmake;
    case Language::Yaml:       return yaml;
    case Language::Toml:       return toml;
    case Language::CSharp:     return csharp;
    case Language::Swift:      return swift;
    case Language::Php:        return php;
    case Language::Kotlin:     return kotlin;
    case Language::Dart:       return dart;
    case Language::Scala:      return scala;
    case Language::Lua:        return lua;
    case Language::Perl:       return perl;
    case Language::R:          return rlang;
    case Language::Haskell:    return haskell;
    case Language::Elixir:     return elixir;
    case Language::OCaml:      return ocaml;
    case Language::FSharp:     return fsharp;
    case Language::Zig:        return zig;
    case Language::Nim:        return nim;
    case Language::Groovy:     return groovy;
    case Language::Julia:      return julia;
    case Language::ObjectiveC: return objectivec;
    case Language::Assembly:   return assembly;
    case Language::Html:
    case Language::Css:
    case Language::Markdown:
    case Language::None:
        break;
    }
    return none;
}

/// Built-in type names, coloured differently from keywords because the design's
/// palette distinguishes them and because it makes a declaration readable at a
/// glance.
const QStringList& typesFor(Language language)
{
    static const QStringList none;

    static const QStringList c = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("char8_t"),
        QStringLiteral("char16_t"), QStringLiteral("char32_t"), QStringLiteral("double"),
        QStringLiteral("float"), QStringLiteral("int"), QStringLiteral("int8_t"),
        QStringLiteral("int16_t"), QStringLiteral("int32_t"), QStringLiteral("int64_t"),
        QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("signed"),
        QStringLiteral("size_t"), QStringLiteral("uint8_t"), QStringLiteral("uint16_t"),
        QStringLiteral("uint32_t"), QStringLiteral("uint64_t"), QStringLiteral("unsigned"),
        QStringLiteral("void"), QStringLiteral("wchar_t"),
    };

    static const QStringList rust = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("f32"),
        QStringLiteral("f64"), QStringLiteral("i8"), QStringLiteral("i16"),
        QStringLiteral("i32"), QStringLiteral("i64"), QStringLiteral("isize"),
        QStringLiteral("str"), QStringLiteral("String"), QStringLiteral("u8"),
        QStringLiteral("u16"), QStringLiteral("u32"), QStringLiteral("u64"),
        QStringLiteral("usize"), QStringLiteral("Vec"),
    };

    static const QStringList go = {
        QStringLiteral("bool"), QStringLiteral("byte"), QStringLiteral("complex64"),
        QStringLiteral("complex128"), QStringLiteral("error"), QStringLiteral("float32"),
        QStringLiteral("float64"), QStringLiteral("int"), QStringLiteral("int8"),
        QStringLiteral("int16"), QStringLiteral("int32"), QStringLiteral("int64"),
        QStringLiteral("rune"), QStringLiteral("string"), QStringLiteral("uint"),
        QStringLiteral("uintptr"),
    };

    static const QStringList java = {
        QStringLiteral("boolean"), QStringLiteral("byte"), QStringLiteral("char"),
        QStringLiteral("double"), QStringLiteral("float"), QStringLiteral("int"),
        QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("void"),
        QStringLiteral("String"), QStringLiteral("Integer"), QStringLiteral("Boolean"),
        QStringLiteral("Double"), QStringLiteral("Long"), QStringLiteral("Object"),
        QStringLiteral("List"), QStringLiteral("Map"), QStringLiteral("Set"),
    };

    /// C# built-ins, plus the handful of framework types that read as
    /// built-in in ordinary code.
    static const QStringList csharpTypes = [] {
        QStringList words = {
            QStringLiteral("bool"), QStringLiteral("byte"), QStringLiteral("char"),
            QStringLiteral("decimal"), QStringLiteral("double"),
            QStringLiteral("dynamic"), QStringLiteral("float"),
            QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("nint"),
            QStringLiteral("nuint"), QStringLiteral("object"),
            QStringLiteral("sbyte"), QStringLiteral("short"),
            QStringLiteral("string"), QStringLiteral("uint"),
            QStringLiteral("ulong"), QStringLiteral("ushort"), QStringLiteral("var"),
            QStringLiteral("void"), QStringLiteral("Task"),
            QStringLiteral("ValueTask"), QStringLiteral("List"),
            QStringLiteral("Dictionary"), QStringLiteral("IEnumerable"),
            QStringLiteral("Span"), QStringLiteral("ReadOnlySpan"),
        };
        words.sort();
        return words;
    }();

    /// Swift's standard types.
    static const QStringList swiftTypes = [] {
        QStringList words = {
            QStringLiteral("Any"), QStringLiteral("AnyObject"),
            QStringLiteral("Array"), QStringLiteral("Bool"),
            QStringLiteral("Character"), QStringLiteral("Dictionary"),
            QStringLiteral("Double"), QStringLiteral("Float"), QStringLiteral("Int"),
            QStringLiteral("Int8"), QStringLiteral("Int16"), QStringLiteral("Int32"),
            QStringLiteral("Int64"), QStringLiteral("Never"),
            QStringLiteral("Optional"), QStringLiteral("Result"),
            QStringLiteral("Set"), QStringLiteral("String"),
            QStringLiteral("Substring"), QStringLiteral("UInt"),
            QStringLiteral("UInt8"), QStringLiteral("UInt16"),
            QStringLiteral("UInt32"), QStringLiteral("UInt64"),
            QStringLiteral("Void"),
        };
        words.sort();
        return words;
    }();

    /// PHP type declarations.
    static const QStringList phpTypes = [] {
        QStringList words = {
            QStringLiteral("array"), QStringLiteral("bool"),
            QStringLiteral("callable"), QStringLiteral("false"),
            QStringLiteral("float"), QStringLiteral("int"),
            QStringLiteral("iterable"), QStringLiteral("mixed"),
            QStringLiteral("never"), QStringLiteral("null"),
            QStringLiteral("object"), QStringLiteral("self"),
            QStringLiteral("static"), QStringLiteral("string"),
            QStringLiteral("true"), QStringLiteral("void"), QStringLiteral("parent"),
        };
        words.sort();
        return words;
    }();

    /// Kotlin's standard types.
    static const QStringList kotlinTypes = [] {
        QStringList words = {
            QStringLiteral("Any"), QStringLiteral("Array"),
            QStringLiteral("Boolean"), QStringLiteral("Byte"),
            QStringLiteral("Char"), QStringLiteral("CharSequence"),
            QStringLiteral("Double"), QStringLiteral("Float"), QStringLiteral("Int"),
            QStringLiteral("List"), QStringLiteral("Long"), QStringLiteral("Map"),
            QStringLiteral("MutableList"), QStringLiteral("MutableMap"),
            QStringLiteral("MutableSet"), QStringLiteral("Nothing"),
            QStringLiteral("Number"), QStringLiteral("Sequence"),
            QStringLiteral("Set"), QStringLiteral("Short"), QStringLiteral("String"),
            QStringLiteral("Unit"),
        };
        words.sort();
        return words;
    }();

    /// Dart's built-in types.
    static const QStringList dartTypes = [] {
        QStringList words = {
            QStringLiteral("bool"), QStringLiteral("double"),
            QStringLiteral("dynamic"), QStringLiteral("Function"),
            QStringLiteral("int"), QStringLiteral("List"), QStringLiteral("Map"),
            QStringLiteral("Never"), QStringLiteral("Null"), QStringLiteral("num"),
            QStringLiteral("Object"), QStringLiteral("Record"),
            QStringLiteral("Set"), QStringLiteral("String"),
            QStringLiteral("Symbol"), QStringLiteral("Type"),
            QStringLiteral("Future"), QStringLiteral("Stream"),
            QStringLiteral("Iterable"), QStringLiteral("void"),
        };
        words.sort();
        return words;
    }();

    /// Scala's standard types.
    static const QStringList scalaTypes = [] {
        QStringList words = {
            QStringLiteral("Any"), QStringLiteral("AnyRef"),
            QStringLiteral("AnyVal"), QStringLiteral("Boolean"),
            QStringLiteral("Byte"), QStringLiteral("Char"), QStringLiteral("Double"),
            QStringLiteral("Either"), QStringLiteral("Float"), QStringLiteral("Int"),
            QStringLiteral("List"), QStringLiteral("Long"), QStringLiteral("Map"),
            QStringLiteral("Nothing"), QStringLiteral("Null"),
            QStringLiteral("Option"), QStringLiteral("Seq"), QStringLiteral("Set"),
            QStringLiteral("Short"), QStringLiteral("String"),
            QStringLiteral("Unit"), QStringLiteral("Vector"),
        };
        words.sort();
        return words;
    }();

    /// Lua's type names, as `type()` reports them.
    static const QStringList luaTypes = [] {
        QStringList words = {
            QStringLiteral("boolean"), QStringLiteral("function"),
            QStringLiteral("nil"), QStringLiteral("number"),
            QStringLiteral("string"), QStringLiteral("table"),
            QStringLiteral("thread"), QStringLiteral("userdata"),
        };
        words.sort();
        return words;
    }();

    /// R's atomic vector types and the common data structures.
    static const QStringList rTypes = [] {
        QStringList words = {
            QStringLiteral("character"), QStringLiteral("complex"),
            QStringLiteral("double"), QStringLiteral("integer"),
            QStringLiteral("logical"), QStringLiteral("numeric"),
            QStringLiteral("raw"), QStringLiteral("data.frame"),
            QStringLiteral("factor"), QStringLiteral("list"),
            QStringLiteral("matrix"), QStringLiteral("vector"),
        };
        words.sort();
        return words;
    }();

    /// Haskell's Prelude types.
    static const QStringList haskellTypes = [] {
        QStringList words = {
            QStringLiteral("Bool"), QStringLiteral("Char"),
            QStringLiteral("Double"), QStringLiteral("Either"),
            QStringLiteral("Float"), QStringLiteral("IO"), QStringLiteral("Int"),
            QStringLiteral("Integer"), QStringLiteral("Maybe"),
            QStringLiteral("Ordering"), QStringLiteral("String"),
            QStringLiteral("Word"), QStringLiteral("Rational"),
        };
        words.sort();
        return words;
    }();

    /// Elixir's typespec names.
    static const QStringList elixirTypes = [] {
        QStringList words = {
            QStringLiteral("atom"), QStringLiteral("binary"),
            QStringLiteral("bitstring"), QStringLiteral("boolean"),
            QStringLiteral("float"), QStringLiteral("fun"),
            QStringLiteral("integer"), QStringLiteral("list"),
            QStringLiteral("map"), QStringLiteral("pid"), QStringLiteral("port"),
            QStringLiteral("reference"), QStringLiteral("tuple"),
        };
        words.sort();
        return words;
    }();

    /// OCaml's built-in types.
    static const QStringList ocamlTypes = [] {
        QStringList words = {
            QStringLiteral("array"), QStringLiteral("bool"),
            QStringLiteral("bytes"), QStringLiteral("char"),
            QStringLiteral("exn"), QStringLiteral("float"),
            QStringLiteral("int"), QStringLiteral("int32"),
            QStringLiteral("int64"), QStringLiteral("list"),
            QStringLiteral("option"), QStringLiteral("string"),
            QStringLiteral("unit"),
        };
        words.sort();
        return words;
    }();

    /// F#'s primitive type abbreviations.
    static const QStringList fsharpTypes = [] {
        QStringList words = {
            QStringLiteral("bool"), QStringLiteral("byte"),
            QStringLiteral("char"), QStringLiteral("decimal"),
            QStringLiteral("double"), QStringLiteral("float"),
            QStringLiteral("int"), QStringLiteral("int64"),
            QStringLiteral("list"), QStringLiteral("obj"),
            QStringLiteral("option"), QStringLiteral("seq"),
            QStringLiteral("single"), QStringLiteral("string"),
            QStringLiteral("unit"),
        };
        words.sort();
        return words;
    }();

    /// Zig's primitive types.
    static const QStringList zigTypes = [] {
        QStringList words = {
            QStringLiteral("bool"), QStringLiteral("c_int"),
            QStringLiteral("c_long"), QStringLiteral("comptime_float"),
            QStringLiteral("comptime_int"), QStringLiteral("f16"),
            QStringLiteral("f32"), QStringLiteral("f64"), QStringLiteral("f128"),
            QStringLiteral("i8"), QStringLiteral("i16"), QStringLiteral("i32"),
            QStringLiteral("i64"), QStringLiteral("i128"),
            QStringLiteral("isize"), QStringLiteral("noreturn"),
            QStringLiteral("type"), QStringLiteral("u8"), QStringLiteral("u16"),
            QStringLiteral("u32"), QStringLiteral("u64"), QStringLiteral("u128"),
            QStringLiteral("usize"), QStringLiteral("void"),
            QStringLiteral("anyerror"), QStringLiteral("anyopaque"),
        };
        words.sort();
        return words;
    }();

    /// Nim's primitive types.
    static const QStringList nimTypes = [] {
        QStringList words = {
            QStringLiteral("bool"), QStringLiteral("byte"),
            QStringLiteral("char"), QStringLiteral("cstring"),
            QStringLiteral("float"), QStringLiteral("float32"),
            QStringLiteral("float64"), QStringLiteral("int"),
            QStringLiteral("int8"), QStringLiteral("int16"),
            QStringLiteral("int32"), QStringLiteral("int64"),
            QStringLiteral("pointer"), QStringLiteral("seq"),
            QStringLiteral("string"), QStringLiteral("uint"),
            QStringLiteral("uint8"), QStringLiteral("uint16"),
            QStringLiteral("uint32"), QStringLiteral("uint64"),
            QStringLiteral("array"), QStringLiteral("openArray"),
        };
        words.sort();
        return words;
    }();

    /// Groovy: Java's primitives plus the common wrappers.
    static const QStringList groovyTypes = [] {
        QStringList words = {
            QStringLiteral("boolean"), QStringLiteral("byte"),
            QStringLiteral("char"), QStringLiteral("double"),
            QStringLiteral("float"), QStringLiteral("int"),
            QStringLiteral("long"), QStringLiteral("short"),
            QStringLiteral("void"), QStringLiteral("BigDecimal"),
            QStringLiteral("String"), QStringLiteral("List"),
            QStringLiteral("Map"),
        };
        words.sort();
        return words;
    }();

    /// Julia's core types.
    static const QStringList juliaTypes = [] {
        QStringList words = {
            QStringLiteral("Any"), QStringLiteral("Array"),
            QStringLiteral("Bool"), QStringLiteral("Char"),
            QStringLiteral("Complex"), QStringLiteral("Dict"),
            QStringLiteral("Float32"), QStringLiteral("Float64"),
            QStringLiteral("Int"), QStringLiteral("Int8"),
            QStringLiteral("Int16"), QStringLiteral("Int32"),
            QStringLiteral("Int64"), QStringLiteral("Nothing"),
            QStringLiteral("Number"), QStringLiteral("Real"),
            QStringLiteral("String"), QStringLiteral("Symbol"),
            QStringLiteral("Tuple"), QStringLiteral("UInt"),
            QStringLiteral("Vector"), QStringLiteral("Matrix"),
        };
        words.sort();
        return words;
    }();

    /// Objective-C: the Foundation types a reader meets constantly.
    static const QStringList objcTypes = [] {
        QStringList words = {
            QStringLiteral("BOOL"), QStringLiteral("Class"),
            QStringLiteral("IMP"), QStringLiteral("NSArray"),
            QStringLiteral("NSDictionary"), QStringLiteral("NSError"),
            QStringLiteral("NSInteger"), QStringLiteral("NSNumber"),
            QStringLiteral("NSObject"), QStringLiteral("NSString"),
            QStringLiteral("NSUInteger"), QStringLiteral("SEL"),
            QStringLiteral("id"), QStringLiteral("instancetype"),
        };
        words.sort();
        return words;
    }();

    /// Registers, coloured as types. They are not types, but they are the
    /// fixed vocabulary of the language - the part that is the machine
    /// rather than the program - and separating them from mnemonics is
    /// what makes a listing scannable.
    static const QStringList asmTypes = [] {
        QStringList words = {
            QStringLiteral("rax"), QStringLiteral("rbx"), QStringLiteral("rcx"),
            QStringLiteral("rdx"), QStringLiteral("rsi"), QStringLiteral("rdi"),
            QStringLiteral("rbp"), QStringLiteral("rsp"), QStringLiteral("r8"),
            QStringLiteral("r9"), QStringLiteral("r10"), QStringLiteral("r11"),
            QStringLiteral("r12"), QStringLiteral("r13"), QStringLiteral("r14"),
            QStringLiteral("r15"), QStringLiteral("eax"), QStringLiteral("ebx"),
            QStringLiteral("ecx"), QStringLiteral("edx"), QStringLiteral("esi"),
            QStringLiteral("edi"), QStringLiteral("ebp"), QStringLiteral("esp"),
            QStringLiteral("ax"), QStringLiteral("bx"), QStringLiteral("cx"),
            QStringLiteral("dx"), QStringLiteral("al"), QStringLiteral("bl"),
            QStringLiteral("cl"), QStringLiteral("dl"), QStringLiteral("ah"),
            QStringLiteral("bh"), QStringLiteral("ch"), QStringLiteral("dh"),
            QStringLiteral("x0"), QStringLiteral("x1"), QStringLiteral("x2"),
            QStringLiteral("x3"), QStringLiteral("x4"), QStringLiteral("x5"),
            QStringLiteral("x6"), QStringLiteral("x7"), QStringLiteral("x8"),
            QStringLiteral("x29"), QStringLiteral("x30"), QStringLiteral("w0"),
            QStringLiteral("w1"), QStringLiteral("w2"), QStringLiteral("w3"),
            QStringLiteral("sp"), QStringLiteral("lr"), QStringLiteral("pc"),
            QStringLiteral("r0"), QStringLiteral("r1"), QStringLiteral("r2"),
            QStringLiteral("r3"), QStringLiteral("r4"), QStringLiteral("r5"),
            QStringLiteral("r6"), QStringLiteral("r7"), QStringLiteral("fp"),
            QStringLiteral("ip"), QStringLiteral("cpsr"),
        };
        words.sort();
        return words;
    }();

    switch (language) {
    case Language::C:    return c;
    case Language::Rust: return rust;
    case Language::Go:   return go;
    case Language::Java: return java;
    case Language::CSharp: return csharpTypes;
    case Language::Swift:  return swiftTypes;
    case Language::Php:    return phpTypes;
    case Language::Kotlin: return kotlinTypes;
    case Language::Dart:   return dartTypes;
    case Language::Scala:  return scalaTypes;
    case Language::Lua:    return luaTypes;
    case Language::R:      return rTypes;
    case Language::Haskell: return haskellTypes;
    case Language::Elixir:  return elixirTypes;
    case Language::OCaml:   return ocamlTypes;
    case Language::FSharp:  return fsharpTypes;
    case Language::Zig:     return zigTypes;
    case Language::Nim:     return nimTypes;
    case Language::Groovy:  return groovyTypes;
    case Language::Julia:   return juliaTypes;
    case Language::ObjectiveC: return objcTypes;
    case Language::Assembly:   return asmTypes;
    default:             break;
    }
    return none;
}

bool isIdentifierStart(QChar character)
{
    return character.isLetter() || character == QLatin1Char('_');
}

bool isIdentifierPart(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

/// Whether a character can appear in a number literal. Deliberately permissive:
/// hex digits, exponents, separators and suffixes all belong to the literal, and
/// splitting them would produce a run of differently-coloured fragments.
bool isNumberPart(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('.')
           || character == QLatin1Char('_') || character == QLatin1Char('\'');
}

/// Brackets and separators: the scaffolding of a line rather than its content.
bool isPunctuation(QChar character)
{
    static const QString punctuation = QStringLiteral("{}()[];,.@");
    return punctuation.contains(character);
}

/// Operators, kept apart from punctuation so that structure stays quiet while
/// the arithmetic and logic in a line stand out.
bool isOperator(QChar character)
{
    static const QString operators = QStringLiteral("=+-*/%<>!&|^~?:");
    return operators.contains(character);
}

/// A name in SCREAMING_SNAKE_CASE, which by convention across every language
/// here means a constant. Two characters minimum, so `A` and `I` stay plain.
bool looksLikeConstant(const QString& word)
{
    if (word.size() < 2) {
        return false;
    }
    bool hasLetter = false;
    for (const QChar character : word) {
        if (character.isLower()) {
            return false;
        }
        hasLetter = hasLetter || character.isLetter();
    }
    return hasLetter;
}

/// Literals that are values rather than actions. Coloured apart from keywords
/// because a condition reads better when its operands and its operators do not
/// share a colour.
bool isConstantWord(const QString& word, Language language)
{
    static const QStringList shared = {
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("nullptr"), QStringLiteral("None"), QStringLiteral("True"),
        QStringLiteral("False"), QStringLiteral("nil"), QStringLiteral("undefined"),
        QStringLiteral("NULL"), QStringLiteral("self"), QStringLiteral("this"),
        QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("on"),
        QStringLiteral("off"),
    };
    Q_UNUSED(language);
    return shared.contains(word);
}

} // namespace

SyntaxHighlighter::SyntaxHighlighter(Language language) : m_language(language) {}

void SyntaxHighlighter::setLanguage(Language language)
{
    m_language = language;
}

const QStringList& SyntaxHighlighter::keywordsOf(Language language)
{
    return keywordsFor(language);
}

const QStringList& SyntaxHighlighter::typesOf(Language language)
{
    return typesFor(language);
}

const QStringList& SyntaxHighlighter::keywords() const
{
    return keywordsFor(m_language);
}

const QStringList& SyntaxHighlighter::types() const
{
    return typesFor(m_language);
}

QString SyntaxHighlighter::lineCommentPrefix() const
{
    switch (m_language) {
    case Language::C:
    case Language::JavaScript:
    case Language::Rust:
    case Language::Go:
    case Language::Qml:
    case Language::TypeScript:
    case Language::Java:
    case Language::Css:
    case Language::CSharp:
    case Language::Swift:
    case Language::Php:      // PHP accepts # too, but // is what is written
    case Language::Kotlin:
    case Language::Dart:
    case Language::Scala:
    case Language::Zig:
    case Language::Groovy:
    case Language::ObjectiveC:
    case Language::FSharp:      // and (* *) for blocks
        return QStringLiteral("//");
    case Language::Python:
    case Language::Shell:
    case Language::Ruby:
    case Language::Yaml:
    case Language::Toml:
    case Language::CMake:
    case Language::Perl:
    case Language::R:
    case Language::Elixir:
    case Language::Nim:      // and #[ ]# for blocks
    case Language::Julia:    // and #= =# for blocks
        return QStringLiteral("#");
    case Language::Sql:
    case Language::Lua:      // and --[[ ]] for blocks
    case Language::Haskell:  // and {- -} for blocks
        return QStringLiteral("--");

    // Assembly's comment character is the one thing every assembler disagrees
    // about. `;` is what NASM, MASM and most x86 listings use; GAS uses `#` on
    // some targets and `//` on others. `;` is the majority and the one a reader
    // of a .asm file expects.
    case Language::Assembly:
        return QStringLiteral(";");

    // OCaml has no line comment at all - (* *) is the only form - so returning
    // nothing here is correct rather than an omission.
    case Language::OCaml:
        break;
    case Language::Html:
    case Language::Markdown:
    case Language::None:
        break;
    }
    return QString();
}

bool SyntaxHighlighter::isCaseInsensitive() const
{
    // Only SQL. Everywhere else `If` is an identifier, and colouring it as a
    // keyword would be wrong rather than generous.
    return m_language == Language::Sql;
}

bool SyntaxHighlighter::isMarkup() const
{
    return m_language == Language::Html;
}

bool SyntaxHighlighter::hasBlockComments() const
{
    switch (m_language) {
    case Language::C:
    case Language::JavaScript:
    case Language::Rust:
    case Language::Go:
    case Language::Qml:
    case Language::TypeScript:
    case Language::Java:
    case Language::Css:
    case Language::Sql:
    case Language::CSharp:
    case Language::Swift:
    case Language::Php:
    case Language::Kotlin:
    case Language::Dart:
    case Language::Scala:
    case Language::Zig:
    case Language::Groovy:
    case Language::ObjectiveC:
        return true;

    // Deliberately absent, each with a block comment that is not /* */:
    // Haskell is {- -}, Nim is #[ ]#, Julia is #= =#, OCaml and F# are (* *),
    // Lua is --[[ ]]. Claiming /* */ for any of them would make a division
    // followed by a dereference open a comment that never closes.

    // Lua's block comment is --[[ ]], not /* */, so it is deliberately absent:
    // claiming support here would make `a / *b` open a comment that never
    // closes and grey out the rest of the file.
    default:
        return false;
    }
}

SyntaxHighlighter::Language SyntaxHighlighter::languageForPath(const QString& path)
{
    // Only extensions with real rules. An unmapped one returns None and the view
    // draws plain text - mis-colouring reads as a bug, no colour reads as a file
    // type Keys does not know yet.
    static const QHash<QString, Language> byExtension = {
        {QStringLiteral("c"), Language::C},
        {QStringLiteral("h"), Language::C},
        {QStringLiteral("cc"), Language::C},
        {QStringLiteral("cpp"), Language::C},
        {QStringLiteral("cxx"), Language::C},
        {QStringLiteral("hpp"), Language::C},
        {QStringLiteral("hxx"), Language::C},
        {QStringLiteral("py"), Language::Python},
        {QStringLiteral("js"), Language::JavaScript},
        {QStringLiteral("jsx"), Language::JavaScript},
        {QStringLiteral("ts"), Language::TypeScript},
        {QStringLiteral("tsx"), Language::TypeScript},
        {QStringLiteral("json"), Language::JavaScript},
        {QStringLiteral("rs"), Language::Rust},
        {QStringLiteral("go"), Language::Go},
        {QStringLiteral("qml"), Language::Qml},
        {QStringLiteral("md"), Language::Markdown},
        {QStringLiteral("sh"), Language::Shell},
        {QStringLiteral("bash"), Language::Shell},
        {QStringLiteral("zsh"), Language::Shell},
        {QStringLiteral("java"), Language::Java},
        {QStringLiteral("rb"), Language::Ruby},
        {QStringLiteral("html"), Language::Html},
        {QStringLiteral("htm"), Language::Html},
        {QStringLiteral("xml"), Language::Html},
        {QStringLiteral("svg"), Language::Html},
        {QStringLiteral("css"), Language::Css},
        {QStringLiteral("scss"), Language::Css},
        {QStringLiteral("yml"), Language::Yaml},
        {QStringLiteral("yaml"), Language::Yaml},
        {QStringLiteral("toml"), Language::Toml},
        {QStringLiteral("ini"), Language::Toml},
        {QStringLiteral("cfg"), Language::Toml},
        {QStringLiteral("sql"), Language::Sql},
        {QStringLiteral("cmake"), Language::CMake},

        {QStringLiteral("cs"), Language::CSharp},
        {QStringLiteral("csx"), Language::CSharp},
        {QStringLiteral("swift"), Language::Swift},
        {QStringLiteral("php"), Language::Php},
        {QStringLiteral("phtml"), Language::Php},
        {QStringLiteral("kt"), Language::Kotlin},
        {QStringLiteral("kts"), Language::Kotlin},
        {QStringLiteral("dart"), Language::Dart},
        {QStringLiteral("scala"), Language::Scala},
        {QStringLiteral("sc"), Language::Scala},
        {QStringLiteral("lua"), Language::Lua},
        {QStringLiteral("pl"), Language::Perl},
        {QStringLiteral("pm"), Language::Perl},
        {QStringLiteral("r"), Language::R},

        {QStringLiteral("hs"), Language::Haskell},
        {QStringLiteral("lhs"), Language::Haskell},
        {QStringLiteral("ex"), Language::Elixir},
        {QStringLiteral("exs"), Language::Elixir},
        {QStringLiteral("ml"), Language::OCaml},
        {QStringLiteral("mli"), Language::OCaml},
        {QStringLiteral("fs"), Language::FSharp},
        {QStringLiteral("fsi"), Language::FSharp},
        {QStringLiteral("fsx"), Language::FSharp},
        {QStringLiteral("zig"), Language::Zig},
        {QStringLiteral("nim"), Language::Nim},
        {QStringLiteral("nims"), Language::Nim},
        {QStringLiteral("groovy"), Language::Groovy},
        {QStringLiteral("gradle"), Language::Groovy},
        {QStringLiteral("jl"), Language::Julia},

        // Assembly. The extension says nothing about the architecture, which is
        // why the rules cover the shape rather than one instruction set.
        {QStringLiteral("asm"), Language::Assembly},
        {QStringLiteral("s"), Language::Assembly},
        {QStringLiteral("inc"), Language::Assembly},
        {QStringLiteral("nasm"), Language::Assembly},

        // A long tail of extensions that map onto rules already present.
        // Every one is here because the file's shape genuinely matches the
        // lexer it points at - a shader is C, a .csproj is XML, a Jupyter
        // notebook is JSON on disk. An extension pointed at the wrong lexer
        // produces confident wrong colour, which reads as a bug; an unmapped
        // one produces plain text, which reads as a type Keys does not know.
        {QStringLiteral("c++"), Language::C},
        {QStringLiteral("cppm"), Language::C},
        {QStringLiteral("ixx"), Language::C},
        {QStringLiteral("tpp"), Language::C},
        {QStringLiteral("txx"), Language::C},
        {QStringLiteral("inl"), Language::C},
        {QStringLiteral("h++"), Language::C},
        {QStringLiteral("hcc"), Language::C},
        {QStringLiteral("def"), Language::C},
        {QStringLiteral("metal"), Language::C},
        {QStringLiteral("glsl"), Language::C},
        {QStringLiteral("vert"), Language::C},
        {QStringLiteral("frag"), Language::C},
        {QStringLiteral("geom"), Language::C},
        {QStringLiteral("comp"), Language::C},
        {QStringLiteral("hlsl"), Language::C},
        {QStringLiteral("fx"), Language::C},
        {QStringLiteral("cginc"), Language::C},
        {QStringLiteral("compute"), Language::C},
        {QStringLiteral("pde"), Language::C},
        {QStringLiteral("vala"), Language::C},
        {QStringLiteral("vapi"), Language::C},
        {QStringLiteral("es6"), Language::JavaScript},
        {QStringLiteral("es"), Language::JavaScript},
        {QStringLiteral("pac"), Language::JavaScript},
        {QStringLiteral("jsonl"), Language::JavaScript},
        {QStringLiteral("ndjson"), Language::JavaScript},
        {QStringLiteral("geojson"), Language::JavaScript},
        {QStringLiteral("webmanifest"), Language::JavaScript},
        {QStringLiteral("avsc"), Language::JavaScript},
        {QStringLiteral("har"), Language::JavaScript},
        {QStringLiteral("ipynb"), Language::JavaScript},
        {QStringLiteral("babelrc"), Language::JavaScript},
        {QStringLiteral("eslintrc"), Language::JavaScript},
        {QStringLiteral("prettierrc"), Language::JavaScript},
        {QStringLiteral("svelte"), Language::Html},
        {QStringLiteral("astro"), Language::Html},
        {QStringLiteral("hbs"), Language::Html},
        {QStringLiteral("handlebars"), Language::Html},
        {QStringLiteral("mustache"), Language::Html},
        {QStringLiteral("ejs"), Language::Html},
        {QStringLiteral("njk"), Language::Html},
        {QStringLiteral("liquid"), Language::Html},
        {QStringLiteral("jinja"), Language::Html},
        {QStringLiteral("jinja2"), Language::Html},
        {QStringLiteral("twig"), Language::Html},
        {QStringLiteral("erb"), Language::Html},
        {QStringLiteral("rss"), Language::Html},
        {QStringLiteral("atom"), Language::Html},
        {QStringLiteral("xsd"), Language::Html},
        {QStringLiteral("xslt"), Language::Html},
        {QStringLiteral("wsdl"), Language::Html},
        {QStringLiteral("storyboard"), Language::Html},
        {QStringLiteral("xib"), Language::Html},
        {QStringLiteral("resx"), Language::Html},
        {QStringLiteral("csproj"), Language::Html},
        {QStringLiteral("vbproj"), Language::Html},
        {QStringLiteral("fsproj"), Language::Html},
        {QStringLiteral("vcxproj"), Language::Html},
        {QStringLiteral("props"), Language::Html},
        {QStringLiteral("targets"), Language::Html},
        {QStringLiteral("nuspec"), Language::Html},
        {QStringLiteral("pom"), Language::Html},
        {QStringLiteral("ui"), Language::Html},
        {QStringLiteral("qrc"), Language::Html},
        {QStringLiteral("kml"), Language::Html},
        {QStringLiteral("gpx"), Language::Html},
        {QStringLiteral("styl"), Language::Css},
        {QStringLiteral("pcss"), Language::Css},
        {QStringLiteral("postcss"), Language::Css},
        {QStringLiteral("sublime-settings"), Language::JavaScript},
        {QStringLiteral("code-workspace"), Language::JavaScript},
        {QStringLiteral("editorconfig"), Language::Toml},
        {QStringLiteral("gitconfig"), Language::Toml},
        {QStringLiteral("npmrc"), Language::Toml},
        {QStringLiteral("flake8"), Language::Toml},
        {QStringLiteral("pylintrc"), Language::Toml},
        {QStringLiteral("coveragerc"), Language::Toml},
        {QStringLiteral("desktop"), Language::Toml},
        {QStringLiteral("service"), Language::Toml},
        {QStringLiteral("reg"), Language::Toml},
        {QStringLiteral("sarif"), Language::JavaScript},
        {QStringLiteral("yamllint"), Language::Yaml},
        {QStringLiteral("cff"), Language::Yaml},
        {QStringLiteral("neon"), Language::Yaml},
        {QStringLiteral("command"), Language::Shell},
        {QStringLiteral("bashrc"), Language::Shell},
        {QStringLiteral("zshrc"), Language::Shell},
        {QStringLiteral("profile"), Language::Shell},
        {QStringLiteral("env"), Language::Shell},
        {QStringLiteral("envrc"), Language::Shell},
        {QStringLiteral("ebuild"), Language::Shell},
        {QStringLiteral("eclass"), Language::Shell},
        {QStringLiteral("install"), Language::Shell},
        {QStringLiteral("mk"), Language::Shell},
        {QStringLiteral("mak"), Language::Shell},
        {QStringLiteral("make"), Language::Shell},
        {QStringLiteral("bazel"), Language::Python},
        {QStringLiteral("bzl"), Language::Python},
        {QStringLiteral("star"), Language::Python},
        {QStringLiteral("gyp"), Language::Python},
        {QStringLiteral("gypi"), Language::Python},
        {QStringLiteral("scons"), Language::Python},
        {QStringLiteral("pyx"), Language::Python},
        {QStringLiteral("pxd"), Language::Python},
        {QStringLiteral("pxi"), Language::Python},
        {QStringLiteral("rpy"), Language::Python},
        {QStringLiteral("sage"), Language::Python},
        {QStringLiteral("tac"), Language::Python},
        {QStringLiteral("ru"), Language::Ruby},
        {QStringLiteral("podspec"), Language::Ruby},
        {QStringLiteral("thor"), Language::Ruby},
        {QStringLiteral("jbuilder"), Language::Ruby},
        {QStringLiteral("arb"), Language::Ruby},
        {QStringLiteral("builder"), Language::Ruby},
        {QStringLiteral("aidl"), Language::Java},
        {QStringLiteral("pde2"), Language::Java},
        {QStringLiteral("ktm"), Language::Kotlin},
        {QStringLiteral("gvy"), Language::Groovy},
        {QStringLiteral("gy"), Language::Groovy},
        {QStringLiteral("gsh"), Language::Groovy},
        {QStringLiteral("sbt"), Language::Scala},
        {QStringLiteral("mill"), Language::Scala},
        {QStringLiteral("tmpl"), Language::Go},
        {QStringLiteral("gotmpl"), Language::Go},
        {QStringLiteral("gohtml"), Language::Go},
        {QStringLiteral("mysql"), Language::Sql},
        {QStringLiteral("pgsql"), Language::Sql},
        {QStringLiteral("tsql"), Language::Sql},
        {QStringLiteral("plsql"), Language::Sql},
        {QStringLiteral("hql"), Language::Sql},
        {QStringLiteral("cql"), Language::Sql},
        {QStringLiteral("prisma"), Language::Sql},
        {QStringLiteral("dml"), Language::Sql},
        {QStringLiteral("graphql"), Language::Sql},
        {QStringLiteral("gql"), Language::Sql},
        {QStringLiteral("mkd"), Language::Markdown},
        {QStringLiteral("mdown"), Language::Markdown},
        {QStringLiteral("mkdn"), Language::Markdown},
        {QStringLiteral("qmd"), Language::Markdown},
        {QStringLiteral("rmd"), Language::Markdown},
        {QStringLiteral("hsc"), Language::Haskell},
        {QStringLiteral("cabal"), Language::Haskell},
        {QStringLiteral("hamlet"), Language::Haskell},
        {QStringLiteral("heex"), Language::Elixir},
        {QStringLiteral("leex"), Language::Elixir},
        {QStringLiteral("eex"), Language::Elixir},
        {QStringLiteral("fsscript"), Language::FSharp},
        {QStringLiteral("asmx"), Language::Assembly},
        {QStringLiteral("a51"), Language::Assembly},
        {QStringLiteral("nas"), Language::Assembly},
        {QStringLiteral("masm"), Language::Assembly},
        {QStringLiteral("gas"), Language::Assembly},
        {QStringLiteral("sx"), Language::Assembly},
        {QStringLiteral("wat"), Language::Assembly},
        {QStringLiteral("wast"), Language::Assembly},
        {QStringLiteral("ll"), Language::Assembly},
        {QStringLiteral("pch"), Language::ObjectiveC},
        {QStringLiteral("nimble"), Language::Nim},
        {QStringLiteral("nimcfg"), Language::Nim},
        {QStringLiteral("zon"), Language::Zig},
        {QStringLiteral("rlang"), Language::R},
        {QStringLiteral("rprofile"), Language::R},
        {QStringLiteral("rhistory"), Language::R},
        {QStringLiteral("t"), Language::Perl},
        {QStringLiteral("pod"), Language::Perl},
        {QStringLiteral("psgi"), Language::Perl},
        {QStringLiteral("cgi"), Language::Perl},
        {QStringLiteral("luau"), Language::Lua},
        {QStringLiteral("rockspec"), Language::Lua},
        {QStringLiteral("nse"), Language::Lua},
        {QStringLiteral("p8"), Language::Lua},
        {QStringLiteral("awk"), Language::Shell},
        {QStringLiteral("sed"), Language::Shell},
        {QStringLiteral("tf"), Language::Toml},
        {QStringLiteral("tfvars"), Language::Toml},
        {QStringLiteral("hcl"), Language::Toml},
        {QStringLiteral("nomad"), Language::Toml},
        {QStringLiteral("swiftinterface"), Language::Swift},
        {QStringLiteral("swiftdoc"), Language::Swift},
        {QStringLiteral("cshtml"), Language::Html},
        {QStringLiteral("razor"), Language::Html},
        {QStringLiteral("vbhtml"), Language::Html},

        // Extensions that map onto rules already present. Each is here because
        // the lexical shape genuinely matches, not to inflate the count: JSONC
        // and JSON5 are JavaScript, and the shell family shares one set.
        {QStringLiteral("mjs"), Language::JavaScript},
        {QStringLiteral("cjs"), Language::JavaScript},
        {QStringLiteral("jsonc"), Language::JavaScript},
        {QStringLiteral("json5"), Language::JavaScript},
        {QStringLiteral("mts"), Language::TypeScript},
        {QStringLiteral("cts"), Language::TypeScript},
        {QStringLiteral("ino"), Language::C},        // Arduino is C++
        {QStringLiteral("cu"), Language::C},         // CUDA
        {QStringLiteral("cuh"), Language::C},
        {QStringLiteral("m"), Language::ObjectiveC},
        {QStringLiteral("mm"), Language::ObjectiveC},
        {QStringLiteral("hh"), Language::C},
        {QStringLiteral("ipp"), Language::C},
        {QStringLiteral("pyi"), Language::Python},
        {QStringLiteral("pyw"), Language::Python},
        {QStringLiteral("rake"), Language::Ruby},
        {QStringLiteral("gemspec"), Language::Ruby},
        {QStringLiteral("fish"), Language::Shell},
        {QStringLiteral("ksh"), Language::Shell},
        {QStringLiteral("less"), Language::Css},
        {QStringLiteral("sass"), Language::Css},
        {QStringLiteral("vue"), Language::Html},
        {QStringLiteral("xhtml"), Language::Html},
        {QStringLiteral("xsl"), Language::Html},
        {QStringLiteral("plist"), Language::Html},
        {QStringLiteral("markdown"), Language::Markdown},
        {QStringLiteral("mdx"), Language::Markdown},
        {QStringLiteral("conf"), Language::Toml},
        {QStringLiteral("properties"), Language::Toml},
        {QStringLiteral("psql"), Language::Sql},
        {QStringLiteral("ddl"), Language::Sql},
    };

    // Files whose name carries the type, with no extension to read. CMakeLists
    // is the one a user of this project sees constantly.
    static const QHash<QString, Language> byName = {
        {QStringLiteral("cmakelists.txt"), Language::CMake},
        {QStringLiteral("dockerfile"), Language::Shell},
        {QStringLiteral("makefile"), Language::Shell},
        {QStringLiteral(".gitignore"), Language::Shell},
        {QStringLiteral(".clangd"), Language::Yaml},
        {QStringLiteral(".clang-format"), Language::Yaml},
        {QStringLiteral("gemfile"), Language::Ruby},
        {QStringLiteral("rakefile"), Language::Ruby},
        {QStringLiteral("podfile"), Language::Ruby},
        {QStringLiteral("gnumakefile"), Language::Shell},
        {QStringLiteral(".bashrc"), Language::Shell},
        {QStringLiteral(".zshrc"), Language::Shell},
        {QStringLiteral(".profile"), Language::Shell},
        {QStringLiteral(".env"), Language::Shell},
        {QStringLiteral(".gitattributes"), Language::Shell},
        {QStringLiteral(".gitmodules"), Language::Toml},
        {QStringLiteral(".editorconfig"), Language::Toml},
        {QStringLiteral(".npmrc"), Language::Toml},
        {QStringLiteral(".dockerignore"), Language::Shell},
        {QStringLiteral(".prettierrc"), Language::JavaScript},
        {QStringLiteral(".babelrc"), Language::JavaScript},
        {QStringLiteral(".eslintrc"), Language::JavaScript},
    };

    if (const auto byFileName =
            byName.constFind(QFileInfo(path).fileName().toLower());
        byFileName != byName.constEnd()) {
        return byFileName.value();
    }

    return byExtension.value(QFileInfo(path).suffix().toLower(), Language::None);
}

/// Markup: `<tag attribute="value">`, comments, and entities.
///
/// Written as its own pass rather than as cases in the identifier loop. In HTML
/// the tag *is* the structure and the text between tags is prose, so running it
/// through rules built for statements would colour ordinary words as if they
/// were code.
void SyntaxHighlighter::tokenizeMarkup(const QString& line, LineState incoming,
                                       LineState& outgoing,
                                       std::vector<Token>& tokens) const
{
    const int length = static_cast<int>(line.size());
    int i = 0;

    // A comment carried in from a previous line runs until it closes.
    if (incoming == LineState::InMarkupComment) {
        const int close = line.indexOf(QStringLiteral("-->"));
        if (close < 0) {
            tokens.push_back({0, length, TokenKind::Comment});
            outgoing = LineState::InMarkupComment;
            return;
        }
        tokens.push_back({0, close + 3, TokenKind::Comment});
        outgoing = LineState::Normal;
        i = close + 3;
    }

    bool inTag = incoming == LineState::InTag;

    while (i < length) {
        const QChar character = line.at(i);

        if (!inTag) {
            if (character == QLatin1Char('<')) {
                // `<!-- ... -->`, which may not close on this line.
                if (line.mid(i, 4) == QLatin1String("<!--")) {
                    const int close = line.indexOf(QStringLiteral("-->"), i + 4);
                    if (close < 0) {
                        tokens.push_back({i, length - i, TokenKind::Comment});
                        outgoing = LineState::InMarkupComment;
                        return;
                    }
                    tokens.push_back({i, close + 3 - i, TokenKind::Comment});
                    i = close + 3;
                    continue;
                }

                // The angle bracket and the element name together, including a
                // closing slash or a `?` for a processing instruction.
                int j = i + 1;
                while (j < length
                       && (line.at(j) == QLatin1Char('/') || line.at(j) == QLatin1Char('!')
                           || line.at(j) == QLatin1Char('?'))) {
                    ++j;
                }
                while (j < length && (isIdentifierPart(line.at(j))
                                      || line.at(j) == QLatin1Char('-')
                                      || line.at(j) == QLatin1Char(':'))) {
                    ++j;
                }
                tokens.push_back({i, j - i, TokenKind::Tag});
                inTag = true;
                i = j;
                continue;
            }

            // An entity is a value, not prose.
            if (character == QLatin1Char('&')) {
                const int close = line.indexOf(QLatin1Char(';'), i);
                if (close > i && close - i < 12) {
                    tokens.push_back({i, close + 1 - i, TokenKind::Constant});
                    i = close + 1;
                    continue;
                }
            }

            // Text content: left plain, which is what prose should be.
            ++i;
            continue;
        }

        // Inside a tag.
        if (character == QLatin1Char('>')) {
            tokens.push_back({i, 1, TokenKind::Tag});
            inTag = false;
            ++i;
            continue;
        }

        if (character == QLatin1Char('/') && i + 1 < length
            && line.at(i + 1) == QLatin1Char('>')) {
            tokens.push_back({i, 2, TokenKind::Tag});
            inTag = false;
            i += 2;
            continue;
        }

        if (character == QLatin1Char('"') || character == QLatin1Char('\'')) {
            const QChar quote = character;
            int j = i + 1;
            while (j < length && line.at(j) != quote) {
                ++j;
            }
            tokens.push_back({i, std::min(j + 1, length) - i, TokenKind::String});
            i = j + 1;
            continue;
        }

        if (isIdentifierStart(character)) {
            int j = i;
            while (j < length && (isIdentifierPart(line.at(j))
                                  || line.at(j) == QLatin1Char('-')
                                  || line.at(j) == QLatin1Char(':'))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Attribute});
            i = j;
            continue;
        }

        if (character == QLatin1Char('=')) {
            tokens.push_back({i, 1, TokenKind::Operator});
        }
        ++i;
    }

    // An element with enough attributes to wrap is ordinary; carry the state.
    outgoing = inTag ? LineState::InTag : LineState::Normal;
}

/// CSS: selectors, properties and values.
///
/// The shape is unlike a programming language - a name before a colon is a
/// property, the text before a brace is a selector, and neither is a keyword -
/// so it gets its own pass rather than a keyword list that would fit badly.
void SyntaxHighlighter::tokenizeCss(const QString& line, LineState incoming,
                                    LineState& outgoing,
                                    std::vector<Token>& tokens) const
{
    const int length = static_cast<int>(line.size());
    int i = 0;

    if (incoming == LineState::InBlockComment) {
        const int close = line.indexOf(QStringLiteral("*/"));
        if (close < 0) {
            tokens.push_back({0, length, TokenKind::Comment});
            outgoing = LineState::InBlockComment;
            return;
        }
        tokens.push_back({0, close + 2, TokenKind::Comment});
        outgoing = LineState::Normal;
        i = close + 2;
    }

    // Whether a name on this line is a property or part of a selector. A colon
    // puts us in a value until the declaration ends.
    bool inValue = false;

    while (i < length) {
        const QChar character = line.at(i);

        if (character == QLatin1Char('/') && i + 1 < length
            && line.at(i + 1) == QLatin1Char('*')) {
            const int close = line.indexOf(QStringLiteral("*/"), i + 2);
            if (close < 0) {
                tokens.push_back({i, length - i, TokenKind::Comment});
                outgoing = LineState::InBlockComment;
                return;
            }
            tokens.push_back({i, close + 2 - i, TokenKind::Comment});
            i = close + 2;
            continue;
        }

        if (character == QLatin1Char('"') || character == QLatin1Char('\'')) {
            const QChar quote = character;
            int j = i + 1;
            while (j < length && line.at(j) != quote) {
                ++j;
            }
            tokens.push_back({i, std::min(j + 1, length) - i, TokenKind::String});
            i = j + 1;
            continue;
        }

        // `#fff`, `#1a2b3c` - a colour is a value, and reads as one. Outside a
        // declaration the same syntax is an id selector.
        if (character == QLatin1Char('#') && i + 1 < length
            && line.at(i + 1).isLetterOrNumber()) {
            int j = i + 1;
            while (j < length && line.at(j).isLetterOrNumber()) {
                ++j;
            }
            tokens.push_back({i, j - i, inValue ? TokenKind::Number : TokenKind::Tag});
            i = j;
            continue;
        }

        if (character.isDigit()
            || (character == QLatin1Char('-') && i + 1 < length
                && line.at(i + 1).isDigit())) {
            int j = i + 1;
            while (j < length && (line.at(j).isLetterOrNumber()
                                  || line.at(j) == QLatin1Char('.')
                                  || line.at(j) == QLatin1Char('%'))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Number});
            i = j;
            continue;
        }

        // `@media`, `@import` - the at-rules, which are the closest CSS has to
        // keywords.
        if (character == QLatin1Char('@')) {
            int j = i + 1;
            while (j < length && isIdentifierPart(line.at(j))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Keyword});
            i = j;
            continue;
        }

        // `.card` is one selector, not a stray dot followed by a name: the
        // leading punctuation is part of what the reader is looking at, so it
        // is consumed with it and coloured like the `#id` form beside it.
        if (character == QLatin1Char('.') && i + 1 < length
            && isIdentifierStart(line.at(i + 1))) {
            int j = i + 1;
            while (j < length && (isIdentifierPart(line.at(j))
                                  || line.at(j) == QLatin1Char('-'))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Tag});
            i = j;
            continue;
        }

        if (isIdentifierStart(character) || character == QLatin1Char('-')) {
            int j = i;
            while (j < length && (isIdentifierPart(line.at(j))
                                  || line.at(j) == QLatin1Char('-'))) {
                ++j;
            }
            if (j == i) {
                ++j;
            }
            tokens.push_back({i, j - i,
                              inValue ? TokenKind::Constant : TokenKind::Attribute});
            i = j;
            continue;
        }

        if (character == QLatin1Char(':')) {
            inValue = true;
            tokens.push_back({i, 1, TokenKind::Operator});
            ++i;
            continue;
        }

        if (character == QLatin1Char(';') || character == QLatin1Char('{')
            || character == QLatin1Char('}')) {
            inValue = false;
            tokens.push_back({i, 1, TokenKind::Punctuation});
            ++i;
            continue;
        }

        if (isPunctuation(character)) {
            tokens.push_back({i, 1, TokenKind::Punctuation});
        }
        ++i;
    }

    outgoing = LineState::Normal;
}

std::vector<Token> SyntaxHighlighter::tokenize(const QString& line, LineState incoming,
                                               LineState& outgoing) const
{
    outgoing = incoming;

    std::vector<Token> tokens;
    if (m_language == Language::None || line.isEmpty()) {
        return tokens;
    }

    // Markup and stylesheets have a shape statements do not; each has its own
    // pass rather than a keyword list bolted onto a loop built for code.
    if (isMarkup()) {
        tokenizeMarkup(line, incoming, outgoing, tokens);
        return tokens;
    }
    if (m_language == Language::Css) {
        tokenizeCss(line, incoming, outgoing, tokens);
        return tokens;
    }

    const int length = static_cast<int>(line.size());
    int i = 0;

    // A block comment carried in from the previous line runs until it closes.
    if (incoming == LineState::InBlockComment) {
        const int close = line.indexOf(QStringLiteral("*/"));
        if (close < 0) {
            tokens.push_back({0, length, TokenKind::Comment});
            return tokens;   // still open at end of line
        }
        tokens.push_back({0, close + 2, TokenKind::Comment});
        outgoing = LineState::Normal;
        i = close + 2;
    }

    const QString linePrefix = lineCommentPrefix();
    const QStringList& keywordList = keywords();
    const QStringList& typeList = types();

    // Markdown is handled separately: it has no keywords, and what matters is
    // its structure - both the line-level kind and the inline spans, which the
    // first version of this ignored entirely. A document is mostly prose, so a
    // highlighter that only marks headings leaves nearly every line plain.
    if (m_language == Language::Markdown) {
        const QString trimmed = line.trimmed();
        const int indent = static_cast<int>(line.size() - line.trimmed().size());

        // A fenced block owns its lines. Everything inside is code in another
        // language, and marking its punctuation as Markdown's would be wrong.
        if (trimmed.startsWith(QLatin1String("```"))
            || trimmed.startsWith(QLatin1String("~~~"))) {
            tokens.push_back({0, length, TokenKind::Comment});
            return tokens;
        }

        // A heading is the line. Bold and links inside one are not marked
        // separately: the heading already reads as the strongest thing there.
        if (trimmed.startsWith(QLatin1Char('#'))) {
            tokens.push_back({0, length, TokenKind::Keyword});
            return tokens;
        }

        // A horizontal rule, before the bullet check - `---` and `***` both
        // start like list markers.
        if (trimmed.size() >= 3
            && (trimmed.count(QLatin1Char('-')) == trimmed.size()
                || trimmed.count(QLatin1Char('*')) == trimmed.size()
                || trimmed.count(QLatin1Char('_')) == trimmed.size())) {
            tokens.push_back({indent, static_cast<int>(trimmed.size()),
                              TokenKind::Punctuation});
            return tokens;
        }

        // The line's marker: a bullet, an ordered number, a quote, or a table
        // row. Marked first so the inline pass below does not treat a leading
        // `*` as the start of emphasis.
        int inlineStart = indent;

        if (trimmed.startsWith(QLatin1Char('>'))) {
            tokens.push_back({indent, 1, TokenKind::Punctuation});
            inlineStart = indent + 1;
        } else if (trimmed.startsWith(QLatin1String("- "))
                   || trimmed.startsWith(QLatin1String("* "))
                   || trimmed.startsWith(QLatin1String("+ "))) {
            tokens.push_back({indent, 1, TokenKind::Punctuation});
            inlineStart = indent + 1;
        } else if (trimmed.at(0).isDigit()) {
            // `1.` or `1)` - an ordered marker, which the first version missed
            // entirely, so every numbered list read as prose.
            int digits = 0;
            while (digits < trimmed.size() && trimmed.at(digits).isDigit()) {
                ++digits;
            }
            if (digits < trimmed.size()
                && (trimmed.at(digits) == QLatin1Char('.')
                    || trimmed.at(digits) == QLatin1Char(')'))) {
                tokens.push_back({indent, digits + 1, TokenKind::Number});
                inlineStart = indent + digits + 1;
            }
        }

        // A table row: the pipes are the structure.
        if (trimmed.startsWith(QLatin1Char('|'))) {
            for (int at = inlineStart; at < length; ++at) {
                if (line.at(at) == QLatin1Char('|')) {
                    tokens.push_back({at, 1, TokenKind::Punctuation});
                }
            }
        }

        // Inline spans. Scanned once, left to right, so an unclosed marker
        // cannot swallow the rest of the line - a `*` in prose is common, and
        // treating it as the start of emphasis that never ends would colour
        // everything after it.
        int at = inlineStart;
        while (at < length) {
            const QChar character = line.at(at);

            // `code`, and ``code with a backtick``.
            if (character == QLatin1Char('`')) {
                int ticks = 0;
                while (at + ticks < length && line.at(at + ticks) == QLatin1Char('`')) {
                    ++ticks;
                }
                const QString fence(ticks, QLatin1Char('`'));
                const int close = line.indexOf(fence, at + ticks);
                if (close > 0) {
                    tokens.push_back({at, close + ticks - at, TokenKind::String});
                    at = close + ticks;
                    continue;
                }
                at += ticks;
                continue;
            }

            // **bold**, __bold__, *italic*, _italic_.
            if (character == QLatin1Char('*') || character == QLatin1Char('_')) {
                const int run = (at + 1 < length && line.at(at + 1) == character) ? 2 : 1;
                const QString marker(run, character);
                const int close = line.indexOf(marker, at + run);
                if (close > 0) {
                    tokens.push_back({at, close + run - at,
                                      run == 2 ? TokenKind::Keyword : TokenKind::Type});
                    at = close + run;
                    continue;
                }
                at += run;
                continue;
            }

            // [text](url) and ![alt](src). The url is what the reader needs to
            // pick out; the text is prose and stays prose.
            if (character == QLatin1Char('[')) {
                const int textEnd = line.indexOf(QLatin1Char(']'), at);
                if (textEnd > 0 && textEnd + 1 < length
                    && line.at(textEnd + 1) == QLatin1Char('(')) {
                    const int urlEnd = line.indexOf(QLatin1Char(')'), textEnd + 1);
                    if (urlEnd > 0) {
                        tokens.push_back({at, textEnd - at + 1, TokenKind::Punctuation});
                        tokens.push_back({textEnd + 1, urlEnd - textEnd,
                                          TokenKind::Constant});
                        at = urlEnd + 1;
                        continue;
                    }
                }
            }

            ++at;
        }

        return tokens;
    }

    // A preprocessor directive owns its line from the `#` to the end of the
    // word. Recognised before the loop because it is only a directive in the
    // leading position - a `#` anywhere else is an operator or a comment.
    if (m_language == Language::C) {
        int lead = 0;
        while (lead < length && line.at(lead).isSpace()) {
            ++lead;
        }
        if (lead < length && line.at(lead) == QLatin1Char('#')) {
            int j = lead + 1;
            while (j < length && line.at(j).isSpace()) {
                ++j;
            }
            while (j < length && isIdentifierPart(line.at(j))) {
                ++j;
            }
            tokens.push_back({lead, j - lead, TokenKind::Preprocessor});
            i = j;
        }
    }

    while (i < length) {
        const QChar character = line.at(i);

        // Line comment: everything to the end.
        if (!linePrefix.isEmpty() && line.mid(i, linePrefix.size()) == linePrefix) {
            tokens.push_back({i, length - i, TokenKind::Comment});
            break;
        }

        // Block comment opening.
        if (hasBlockComments() && character == QLatin1Char('/')
            && i + 1 < length && line.at(i + 1) == QLatin1Char('*')) {
            const int close = line.indexOf(QStringLiteral("*/"), i + 2);
            if (close < 0) {
                tokens.push_back({i, length - i, TokenKind::Comment});
                outgoing = LineState::InBlockComment;
                break;
            }
            tokens.push_back({i, close + 2 - i, TokenKind::Comment});
            i = close + 2;
            continue;
        }

        // Strings. The escape handling matters: "a\"b" is one string, and a
        // highlighter that stopped at the escaped quote would colour the rest of
        // the line wrongly.
        if (character == QLatin1Char('"') || character == QLatin1Char('\'')
            || (m_language == Language::JavaScript && character == QLatin1Char('`'))) {
            const QChar quote = character;
            int j = i + 1;
            while (j < length) {
                if (line.at(j) == QLatin1Char('\\')) {
                    j += 2;   // skip the escaped character
                    continue;
                }
                if (line.at(j) == quote) {
                    ++j;
                    break;
                }
                ++j;
            }
            tokens.push_back({i, std::min(j, length) - i, TokenKind::String});
            i = j;
            continue;
        }

        // Numbers, including hex and floats.
        if (character.isDigit()) {
            int j = i;
            while (j < length && isNumberPart(line.at(j))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Number});
            i = j;
            continue;
        }

        // Identifiers: keyword, type, function call, or plain.
        //
        // Two languages extend what may start one. In Objective-C the
        // @-directives - @interface, @property, @end - are the language's own
        // structure, and `@` is not an identifier character, so without this
        // they tokenised as punctuation followed by an ordinary word and none
        // of them was ever marked. In assembly a leading `.` marks a directive
        // or a local label the same way.
        const bool startsDirective =
            (m_language == Language::ObjectiveC && character == QLatin1Char('@')
             && i + 1 < length && isIdentifierStart(line.at(i + 1)))
            || (m_language == Language::Assembly && character == QLatin1Char('.')
                && i + 1 < length && isIdentifierStart(line.at(i + 1)));

        if (isIdentifierStart(character) || startsDirective) {
            int j = startsDirective ? i + 1 : i;
            while (j < length && isIdentifierPart(line.at(j))) {
                ++j;
            }
            const QString word = line.mid(i, j - i);

            const Qt::CaseSensitivity sensitivity =
                isCaseInsensitive() ? Qt::CaseInsensitive : Qt::CaseSensitive;

            TokenKind kind = TokenKind::Plain;
            if (isConstantWord(word, m_language)) {
                // Before the keyword test: `true` and `nullptr` are reserved
                // words in most of these languages, but they read as values.
                kind = TokenKind::Constant;
            } else if (keywordList.contains(word, sensitivity)) {
                kind = TokenKind::Keyword;
            } else if (typeList.contains(word, sensitivity)) {
                kind = TokenKind::Type;
            } else if (looksLikeConstant(word)) {
                kind = TokenKind::Constant;
            } else {
                // A call, if the next non-space character opens a paren. This is
                // a heuristic, and a cheap one - but it is right far more often
                // than not, and it is what makes code scannable.
                int k = j;
                while (k < length && line.at(k).isSpace()) {
                    ++k;
                }
                if (k < length && line.at(k) == QLatin1Char('(')) {
                    kind = TokenKind::Function;
                }
            }

            if (kind != TokenKind::Plain) {
                tokens.push_back({i, j - i, kind});
            }
            i = j;
            continue;
        }

        if (isPunctuation(character)) {
            tokens.push_back({i, 1, TokenKind::Punctuation});
            ++i;
            continue;
        }

        // Operators run together: `!=`, `&&`, `->`, `<=>` are each one thing,
        // and colouring them character by character would flicker.
        if (isOperator(character)) {
            int j = i;
            while (j < length && isOperator(line.at(j))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Operator});
            i = j;
            continue;
        }

        ++i;
    }

    return tokens;
}

} // namespace keys::editor
