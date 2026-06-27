// DocMind AI — ChunkingStrategy 实现
// TitleLengthChunker: 维护标题路径栈，按目标长度累积正文块。
#include "knowledge/ChunkingStrategy.h"
#include "utils/Logger.h"

#include <QString>
#include <QStringList>
#include <QStack>
#include <QRegularExpression>
#include <algorithm>

namespace dmc::knowledge {

namespace {

// 标题路径栈 -> 文本路径
QString buildHeadingPath(const QStack<std::pair<int, QString>>& stack) {
    if (stack.isEmpty()) return {};
    QStringList parts;
    for (const auto& [lvl, txt] : stack) parts << txt;
    return parts.join(QStringLiteral(" > "));
}

// 把一段过长的文本硬切成不超过 maxChars 的若干段
QStringList splitHard(const QString& text, int maxChars) {
    QStringList out;
    int pos = 0;
    while (pos < text.size()) {
        int len = std::min(maxChars, int(text.size() - pos));
        // 优先在换行/句号处切
        if (pos + len < text.size()) {
            int cutLine = text.lastIndexOf(QLatin1Char('\n'), pos + len);
            int cutDot  = text.lastIndexOf(QStringLiteral("。"), pos + len);
            int cut = std::max({cutLine, cutDot});
            if (cut > pos + maxChars / 2) len = cut - pos + 1;
        }
        out << text.mid(pos, len).trimmed();
        pos += len;
    }
    return out;
}

// 取当前块共享的来源定位（取第一个块的页码/行号作为代表）
void applySource(Chunk& c, const StructBlock& representative) {
    c.sourcePage = representative.sourcePage;
    c.sourceLine = representative.sourceLine;
}

} // namespace

QList<Chunk> TitleLengthChunker::chunk(const QList<StructBlock>& blocks,
                                         const ChunkingOptions& opts) const {
    QList<Chunk> out;

    QStack<std::pair<int, QString>> headingStack; // (level, text)
    QString currentHeadingPath;
    int currentHeadingLevel = 0;

    // 当前正在累积的块
    QList<StructBlock> buffer;
    int bufferChars = 0;
    StructBlock bufferHead; // 用于 sourcePage/Line 代表
    bool bufferHasHead = false;

    auto flushBuffer = [&]() {
        if (buffer.isEmpty()) return;
        // 合并文本
        QStringList parts;
        for (const StructBlock& b : buffer) parts << b.text.trimmed();
        QString merged = parts.join(QStringLiteral("\n\n"));

        // 长度未超上限：单块
        if (merged.size() <= opts.maxChars) {
            Chunk c;
            c.text = merged;
            c.headingLevel = currentHeadingLevel;
            c.headingPath  = currentHeadingPath;
            applySource(c, bufferHasHead ? bufferHead : buffer.first());
            out.append(c);
        } else {
            // 硬切：保留 headingPath
            for (const QString& seg : splitHard(merged, opts.maxChars)) {
                if (seg.isEmpty()) continue;
                Chunk c;
                c.text = seg;
                c.headingLevel = currentHeadingLevel;
                c.headingPath  = currentHeadingPath;
                applySource(c, bufferHasHead ? bufferHead : buffer.first());
                out.append(c);
            }
        }
        buffer.clear();
        bufferChars = 0;
        bufferHasHead = false;
    };

    for (const StructBlock& b : blocks) {
        // 标题：更新栈，但不立即 flush（让后续段落带上新标题）
        if (b.type == BlockType::Heading) {
            // 弹出级别 >= 当前的
            while (!headingStack.isEmpty() && headingStack.top().first >= b.level) {
                headingStack.pop();
            }
            headingStack.push({b.level, b.text.trimmed()});
            currentHeadingLevel = b.level;
            currentHeadingPath  = buildHeadingPath(headingStack);
            // 若缓冲已有内容且开启了"标题独立成块"语义，可在这里 flush。
            // 当前实现：让缓冲继续累积，直到长度触发或下个标题（见下）
            continue;
        }

        // 累积达到 targetChars 且即将超出 maxChars 时切出
        if (bufferChars + b.text.size() > opts.maxChars && !buffer.isEmpty()) {
            flushBuffer();
        }

        if (buffer.isEmpty()) {
            bufferHead = b;
            bufferHasHead = true;
        }
        buffer.append(b);
        bufferChars += b.text.size() + 2; // 段间分隔

        // 达到目标即切（避免过长累积）
        if (bufferChars >= opts.targetChars) {
            flushBuffer();
        }
    }
    flushBuffer();

    // 编号
    for (int i = 0; i < out.size(); ++i) out[i].ordinal = i;

    LOG_INFO("Chunking", QString("Blocks=%1 -> Chunks=%2")
                          .arg(blocks.size()).arg(out.size()));
    return out;
}

} // namespace dmc::knowledge
