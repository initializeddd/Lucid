import AppKit
import Foundation
let size = NSSize(width: 760, height: 450)
let image = NSImage(size: size)
image.lockFocus()
let rect = NSRect(origin: .zero, size: size)
let gradient = NSGradient(starting: NSColor(calibratedRed: 0.055, green: 0.06, blue: 0.10, alpha: 1), ending: NSColor(calibratedRed: 0.16, green: 0.14, blue: 0.29, alpha: 1))!
gradient.draw(in: rect, angle: 20)
let title: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 38, weight: .bold), .foregroundColor: NSColor.white]
let detail: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 16, weight: .regular), .foregroundColor: NSColor(calibratedWhite: 0.75, alpha: 1)]
("LUCID" as NSString).draw(at: NSPoint(x: 52, y: 365), withAttributes: title)
("A focused space for your code." as NSString).draw(at: NSPoint(x: 53, y: 334), withAttributes: detail)
("Drag Lucid into Applications to install" as NSString).draw(at: NSPoint(x: 210, y: 46), withAttributes: detail)
("→" as NSString).draw(at: NSPoint(x: 361, y: 193), withAttributes: [.font: NSFont.systemFont(ofSize: 36), .foregroundColor: NSColor(calibratedRed: 0.57, green: 0.55, blue: 1, alpha: 1)])
image.unlockFocus()
let png = NSBitmapImageRep(data: image.tiffRepresentation!)!.representation(using: .png, properties: [:])!
try png.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
