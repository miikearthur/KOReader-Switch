// Renders KOReader for Switch icon variants from KOReader's vector logo.
// usage: swift make_icon.swift <resources/koreader.svg> <output dir>
//
// `platform/switch/icon.jpg` is "a-teal-wordmark": icon-a-teal-wordmark-1024.png, scaled to
// 256x256 (`sips -z 256 256`), then encoded by png2jpg.c. ImageIO's own JPEGs carry EXIF and
// Photoshop segments, which don't belong in a HOME menu icon.
import AppKit
import Foundation
import ImageIO
import UniformTypeIdentifiers

let svgPath = CommandLine.arguments[1]
let outDir = CommandLine.arguments[2]
let sRGB = CGColorSpace(name: CGColorSpace.sRGB)!

func rgb(_ hex: UInt32, _ alpha: CGFloat = 1) -> NSColor {
    NSColor(srgbRed: CGFloat((hex >> 16) & 0xff) / 255, green: CGFloat((hex >> 8) & 0xff) / 255,
            blue: CGFloat(hex & 0xff) / 255, alpha: alpha)
}

/// KOReader's logo, with its layers recolored: back cover, page edge, front cover, "KO" letters.
func logo(back: String, page: String, front: String, letters: String) -> NSImage {
    var svg = try! String(contentsOfFile: svgPath, encoding: .utf8)
    let fills = ["path96": back, "path98": page, "path100": front,
                 "path102": letters, "path108": letters, "path114": letters]
    for (id, fill) in fills {
        let re = try! NSRegularExpression(pattern: "(id=\"\(id)\"[\\s\\S]*?fill:)#[0-9a-fA-F]{6}")
        svg = re.stringByReplacingMatches(in: svg, range: NSRange(svg.startIndex..., in: svg), withTemplate: "$1\(fill)")
    }
    return NSImage(data: svg.data(using: .utf8)!)!
}

func render(_ size: Int, _ body: (CGContext, CGFloat) -> Void) -> CGImage {
    let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0,
                        space: sRGB, bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: false)
    body(ctx, CGFloat(size))
    NSGraphicsContext.restoreGraphicsState()
    return ctx.makeImage()!
}

func tealBackground(_ c: CGContext, _ S: CGFloat) {
    let g = CGGradient(colorsSpace: sRGB, colors: [rgb(0x22C4B6).cgColor, rgb(0x009A8F).cgColor, rgb(0x005E57).cgColor] as CFArray,
                       locations: [0, 0.5, 1])!
    c.drawLinearGradient(g, start: CGPoint(x: S * 0.15, y: S), end: CGPoint(x: S * 0.85, y: 0), options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
    let glow = CGGradient(colorsSpace: sRGB, colors: [rgb(0xFFFFFF, 0.30).cgColor, rgb(0xFFFFFF, 0).cgColor] as CFArray, locations: [0, 1])!
    let center = CGPoint(x: S * 0.5, y: S * 0.62)
    c.drawRadialGradient(glow, startCenter: center, startRadius: 0, endCenter: center, endRadius: S * 0.52, options: [])
}

func paperBackground(_ c: CGContext, _ S: CGFloat) {
    c.setFillColor(rgb(0xF4EFE4).cgColor)
    c.fill(CGRect(x: 0, y: 0, width: S, height: S))
    let g = CGGradient(colorsSpace: sRGB, colors: [rgb(0xFFFDF8).cgColor, rgb(0xE6DCC8).cgColor] as CFArray, locations: [0, 1])!
    let center = CGPoint(x: S * 0.5, y: S * 0.58)
    c.drawRadialGradient(g, startCenter: center, startRadius: 0, endCenter: center, endRadius: S * 0.78, options: [.drawsAfterEndLocation])
}

func drawLogo(_ c: CGContext, _ image: NSImage, center: CGPoint, size: CGFloat, shadow: NSColor) {
    c.saveGState()
    c.setShadow(offset: CGSize(width: 0, height: -size * 0.035), blur: size * 0.08, color: shadow.cgColor)
    c.beginTransparencyLayer(auxiliaryInfo: nil)
    image.draw(in: CGRect(x: center.x - size / 2, y: center.y - size / 2, width: size, height: size))
    c.endTransparencyLayer()
    c.restoreGState()
}

func drawWordmark(_ c: CGContext, _ S: CGFloat, baseline: CGFloat, size: CGFloat, color: NSColor, shadow: NSColor?) {
    var font = NSFont.systemFont(ofSize: size, weight: .bold)
    if let d = font.fontDescriptor.withDesign(.rounded), let f = NSFont(descriptor: d, size: size) { font = f }
    let text = NSAttributedString(string: "KOReader", attributes: [.font: font, .foregroundColor: color, .kern: -size * 0.01])
    c.saveGState()
    if let shadow { c.setShadow(offset: CGSize(width: 0, height: -size * 0.03), blur: size * 0.12, color: shadow.cgColor) }
    text.draw(at: CGPoint(x: (S - text.size().width) / 2, y: baseline + font.descender))
    c.restoreGState()
}

let whiteBook = logo(back: "#8FD3CB", page: "#CBEDE8", front: "#FFFFFF", letters: "#00A89C")
let tealBook = logo(back: "#00A89C", page: "#FFFFFF", front: "#00A89C", letters: "#FFFFFF")

let variants: [(String, (CGContext, CGFloat) -> Void)] = [
    ("a-teal-wordmark", { c, S in
        tealBackground(c, S)
        drawLogo(c, whiteBook, center: CGPoint(x: S * 0.5, y: S * 0.575), size: S * 0.64, shadow: rgb(0x00302C, 0.45))
        drawWordmark(c, S, baseline: S * 0.095, size: S * 0.125, color: .white, shadow: rgb(0x00302C, 0.35))
    }),
    ("b-teal-logo", { c, S in
        tealBackground(c, S)
        drawLogo(c, whiteBook, center: CGPoint(x: S * 0.5, y: S * 0.5), size: S * 0.78, shadow: rgb(0x00302C, 0.45))
    }),
    ("c-paper-wordmark", { c, S in
        paperBackground(c, S)
        drawLogo(c, tealBook, center: CGPoint(x: S * 0.5, y: S * 0.575), size: S * 0.64, shadow: rgb(0x4A3A1A, 0.28))
        drawWordmark(c, S, baseline: S * 0.095, size: S * 0.125, color: rgb(0x00796F), shadow: nil)
    }),
]

func writeImage(_ image: CGImage, _ path: String, _ type: UTType, quality: CGFloat = 0.93) {
    let dest = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL, type.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(dest, image, [kCGImageDestinationLossyCompressionQuality: quality] as CFDictionary)
    CGImageDestinationFinalize(dest)
}

// Contact sheet: each variant large (HOME menu tile), plus small (All Software grid).
let big = 400, small = 120, gap = 24
let sheetW = variants.count * (big + gap) + gap, sheetH = big + small + 3 * gap
let sheet = CGContext(data: nil, width: sheetW, height: sheetH, bitsPerComponent: 8, bytesPerRow: 0,
                      space: sRGB, bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
sheet.setFillColor(rgb(0x2D2D2D).cgColor)
sheet.fill(CGRect(x: 0, y: 0, width: sheetW, height: sheetH))
for (i, (name, draw)) in variants.enumerated() {
    let x = gap + i * (big + gap)
    sheet.draw(render(big, draw), in: CGRect(x: x, y: small + 2 * gap, width: big, height: big))
    sheet.draw(render(small, draw), in: CGRect(x: x, y: gap, width: small, height: small))
    writeImage(render(256, draw), "\(outDir)/icon-\(name).jpg", .jpeg)
    writeImage(render(1024, draw), "\(outDir)/icon-\(name)-1024.png", .png)
}
writeImage(sheet.makeImage()!, "\(outDir)/icon-variants.png", .png)
print("rendered", variants.map { $0.0 })
