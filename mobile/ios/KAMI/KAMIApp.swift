import SwiftUI

@main
struct KAMIApp: App {
    @StateObject private var session = PreviewSession()
    var body: some Scene {
        WindowGroup {
            RootView().environmentObject(session).preferredColorScheme(.dark)
        }
    }
}
