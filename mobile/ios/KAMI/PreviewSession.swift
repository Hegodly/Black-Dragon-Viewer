import Foundation
import Combine

struct PreviewMessage: Identifiable {
    let id = UUID()
    let text: String
    let date = Date()
}

/// Local prototype state. No credentials, server connection, or delivery claims.
@MainActor
final class PreviewSession: ObservableObject {
    @Published private(set) var messages: [PreviewMessage] = []
    @Published var positionX: Float = 0
    @Published var positionZ: Float = 0
    @Published var heading: Float = 0

    @discardableResult
    func addLocalMessage(_ draft: String) -> Bool {
        let text = draft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty, text.utf8.count <= 1024 else { return false }
        messages.append(PreviewMessage(text: text))
        return true
    }
    func move(_ distance: Float) {
        positionX = min(12, max(-12, positionX + sin(heading) * distance))
        positionZ = min(12, max(-12, positionZ - cos(heading) * distance))
    }
    func reset() {
        messages.removeAll()
        positionX = 0; positionZ = 0; heading = 0
    }
}
