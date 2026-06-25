// DocMind AI — ConversionEngineExtractionAdapter
// 把模块 B 的 dmc::conversion::ConversionEngine 同步 extractText 适配到模块 C 的 ExtractionAdapter。
//
// 设计：直接调用 B 的同步 extractText（不阻塞 UI 由 KnowledgeIngestionService 负责
//      通过 QtConcurrent 转入 worker 线程）。这一层只做类型转换，无 IO 重负。
#pragma once

#include "knowledge/KnowledgeIngestionService.h"
#include "conversion/ConversionService.h"

#include <QString>

namespace dmc::knowledge {

class ConversionEngineExtractionAdapter : public ExtractionAdapter {
public:
    explicit ConversionEngineExtractionAdapter(conversion::ConversionEngine* engine)
        : m_engine(engine) {}

    ExtractionOutput extract(const ExtractionInput& in) override {
        ExtractionOutput out;
        if (!m_engine) {
            out.ok = false;
            out.errorMessage = QStringLiteral("ConversionEngine not available");
            return out;
        }

        conversion::TextExtractionRequest req;
        req.source_path     = in.sourcePath;
        req.source_content  = in.sourceContent;
        req.source_format   = in.sourceFormat;
        req.prefer_structure= in.preferStructure;
        req.layout_mode     = conversion::TextExtractionRequest::Raw;

        conversion::TextExtractionResult r = m_engine->extractText(req);
        out.ok             = r.ok;
        out.markdownText   = r.markdown_text;
        out.plainText      = r.plain_text;
        out.errorMessage   = r.error;
        out.errorCode      = int(r.error_code);

        // 转换 StructBlock
        out.blocks.reserve(r.blocks.size());
        for (const conversion::StructBlock& b : r.blocks) {
            StructBlock nb;
            switch (b.type) {
                case conversion::StructBlock::Heading:    nb.type = BlockType::Heading; break;
                case conversion::StructBlock::Paragraph:   nb.type = BlockType::Paragraph; break;
                case conversion::StructBlock::ListItem:    nb.type = BlockType::ListItem; break;
                case conversion::StructBlock::CodeBlock:   nb.type = BlockType::CodeBlock; break;
                case conversion::StructBlock::TableCell:   nb.type = BlockType::TableCell; break;
                case conversion::StructBlock::Blockquote:  nb.type = BlockType::Blockquote; break;
            }
            nb.level      = b.level;
            nb.text       = b.text;
            nb.sourceLine = b.sourceLine;
            nb.sourcePage = b.sourcePage;
            out.blocks.append(nb);
        }

        // 转换 SourceSpan
        out.spans.reserve(r.spans.size());
        for (const conversion::SourceSpan& s : r.spans) {
            SourceSpan ns;
            ns.page      = s.page;
            ns.lineStart = s.lineStart;
            ns.charStart = s.charStart;
            ns.anchor    = s.anchor;
            out.spans.append(ns);
        }
        return out;
    }

private:
    conversion::ConversionEngine* m_engine;
};

} // namespace dmc::knowledge
