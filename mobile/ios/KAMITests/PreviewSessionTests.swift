import XCTest
@testable import KAMI

final class PreviewSessionTests: XCTestCase {
    @MainActor func testRejectsBlankAndOversizeMessages() {
        let model = PreviewSession()
        XCTAssertFalse(model.addLocalMessage(" \n "))
        XCTAssertFalse(model.addLocalMessage(String(repeating: "🙂", count: 257)))
        XCTAssertTrue(model.messages.isEmpty)
        XCTAssertTrue(model.addLocalMessage(" hello "))
        XCTAssertEqual(model.messages.first?.text, "hello")
    }
    @MainActor func testMovementRespectsHeadingAndBounds() {
        let model = PreviewSession()
        model.move(1)
        XCTAssertEqual(model.positionZ, -1, accuracy: 0.001)
        model.heading = .pi / 2
        model.move(1)
        XCTAssertEqual(model.positionX, 1, accuracy: 0.001)
        for _ in 0..<100 { model.move(1) }
        XCTAssertEqual(model.positionX, 12, accuracy: 0.001)
        model.reset()
        XCTAssertEqual(model.positionX, 0)
        XCTAssertEqual(model.positionZ, 0)
        XCTAssertEqual(model.heading, 0)
    }
}
