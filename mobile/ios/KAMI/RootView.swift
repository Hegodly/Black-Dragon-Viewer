import SwiftUI

struct RootView: View {
    @EnvironmentObject private var session: PreviewSession
    var body: some View {
        TabView {
            WorldView().tabItem { Label("World", systemImage: "globe.americas.fill") }
            MessagesView().tabItem { Label("Messages", systemImage: "bubble.left.and.bubble.right.fill") }
            NavigationStack {
                List {
                    Section("KAMI · Beyond Reality") {
                        Text("iPhone development preview").font(.headline)
                        Text("This build contains a local 3D scene and a local message composer. It is not connected to Second Life.")
                    }
                    Section("Connection") {
                        Label("Second Life login — not implemented", systemImage: "network.slash")
                        Text("Live IMs, inventory, avatars, region streaming and movement synchronization require the Second Life protocol client. No password is requested or stored by this preview.")
                    }
                    Section("Preview data") {
                        Text("Messages stay in memory and are never sent. Closing the process clears them.")
                        Button("Reset local preview", role: .destructive) { session.reset() }
                    }
                    Section("Project") {
                        Text("KAMI for iPhone is a separate client. The Windows viewer executable cannot run on iOS.")
                    }
                }.navigationTitle("KAMI")
            }.tabItem { Label("Settings", systemImage: "gearshape.fill") }
        }.tint(.orange)
    }
}

struct MessagesView: View {
    @EnvironmentObject private var session: PreviewSession
    @State private var draft = ""
    @State private var error = false
    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                Text("LOCAL PREVIEW · Messages are not sent to Second Life")
                    .font(.caption).foregroundStyle(.orange).padding()
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(alignment: .trailing, spacing: 12) {
                            if session.messages.isEmpty {
                                Text("Try the KAMI message composer. Live conversations will be added with the network client.")
                                    .foregroundStyle(.secondary).padding()
                            }
                            ForEach(session.messages) { message in
                                VStack(alignment: .trailing, spacing: 4) {
                                    Text(message.text).padding(12).background(.blue, in: RoundedRectangle(cornerRadius: 18))
                                    Text("Local only").font(.caption2).foregroundStyle(.secondary)
                                }.frame(maxWidth: .infinity, alignment: .trailing).id(message.id)
                            }
                        }.padding()
                    }.onChange(of: session.messages.count) { _ in
                        if let id = session.messages.last?.id { proxy.scrollTo(id, anchor: .bottom) }
                    }
                }
                if error { Text("Enter 1–1,024 UTF-8 bytes.").font(.caption).foregroundStyle(.red) }
                HStack(alignment: .bottom) {
                    TextField("Local message", text: $draft, axis: .vertical)
                        .lineLimit(1...5).textFieldStyle(.roundedBorder)
                    Button {
                        if session.addLocalMessage(draft) { draft = ""; error = false }
                        else { error = true }
                    } label: { Image(systemName: "arrow.up.circle.fill").font(.title) }
                        .accessibilityLabel("Add local message")
                }.padding().background(.ultraThinMaterial)
            }.navigationTitle("KAMI Messenger")
        }
    }
}
