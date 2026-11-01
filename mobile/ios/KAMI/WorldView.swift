import SwiftUI
import SceneKit

struct WorldView: View {
    @EnvironmentObject private var session: PreviewSession
    var body: some View {
        ZStack {
            GardenScene(x: session.positionX, z: session.positionZ, heading: session.heading)
                .ignoresSafeArea(edges: .top)
            VStack {
                VStack(spacing: 4) {
                    Text("K A M I").font(.largeTitle.bold())
                    Text("BEYOND REALITY").font(.caption).tracking(3)
                    Text("LOCAL 3D PREVIEW · Not a Second Life region")
                        .font(.caption2).foregroundStyle(.orange)
                }.padding().background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 20))
                Spacer()
                Text("Use arrows to walk and turn. Drag the scene to orbit.")
                    .font(.caption).padding(8).background(.ultraThinMaterial, in: Capsule())
                HStack(spacing: 20) {
                    control("Turn left", "arrow.turn.up.left") { session.heading -= .pi / 8 }
                    VStack {
                        control("Walk forward", "arrow.up") { session.move(0.7) }
                        control("Walk backward", "arrow.down") { session.move(-0.7) }
                    }
                    control("Turn right", "arrow.turn.up.right") { session.heading += .pi / 8 }
                }.padding().background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 24))
            }.padding()
        }
    }
    private func control(_ label: String, _ symbol: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: symbol).font(.title2).frame(width: 54, height: 44)
        }.buttonStyle(.bordered).tint(.orange).accessibilityLabel(label)
    }
}

struct GardenScene: UIViewRepresentable {
    let x: Float
    let z: Float
    let heading: Float
    final class Coordinator {
        let avatar = SCNNode()
    }
    func makeCoordinator() -> Coordinator { Coordinator() }
    func makeUIView(context: Context) -> SCNView {
        let view = SCNView()
        let scene = SCNScene()
        scene.background.contents = UIColor(red: 0.025, green: 0.03, blue: 0.055, alpha: 1)
        let ground = SCNNode(geometry: SCNFloor())
        ground.geometry?.firstMaterial?.diffuse.contents = UIColor(white: 0.12, alpha: 1)
        ground.geometry?.firstMaterial?.roughness.contents = 0.9
        scene.rootNode.addChildNode(ground)
        let avatar = context.coordinator.avatar
        avatar.geometry = SCNCapsule(capRadius: 0.3, height: 1.7)
        avatar.geometry?.firstMaterial?.diffuse.contents = UIColor.systemOrange
        avatar.position = SCNVector3(0, 0.85, 0)
        scene.rootNode.addChildNode(avatar)
        for side in [-1, 1] {
            for index in 0..<6 {
                let lantern = SCNNode(geometry: SCNBox(width: 0.5, height: 0.8, length: 0.5, chamferRadius: 0.08))
                lantern.position = SCNVector3(Float(side) * 3, 0.65, Float(index) * -3 + 5)
                lantern.geometry?.firstMaterial?.diffuse.contents = UIColor.systemRed
                lantern.geometry?.firstMaterial?.emission.contents = UIColor(red: 0.5, green: 0.12, blue: 0.015, alpha: 1)
                scene.rootNode.addChildNode(lantern)
            }
        }
        let ambient = SCNNode(); ambient.light = SCNLight(); ambient.light?.type = .ambient
        ambient.light?.intensity = 400; scene.rootNode.addChildNode(ambient)
        let sun = SCNNode(); sun.light = SCNLight(); sun.light?.type = .directional
        sun.light?.intensity = 900; sun.eulerAngles = SCNVector3(-0.8, -0.5, 0)
        scene.rootNode.addChildNode(sun)
        let camera = SCNNode(); camera.camera = SCNCamera()
        camera.position = SCNVector3(0, 7, 13); camera.look(at: SCNVector3(0, 0, -2))
        scene.rootNode.addChildNode(camera)
        view.scene = scene; view.pointOfView = camera
        view.allowsCameraControl = true; view.preferredFramesPerSecond = 30
        view.antialiasingMode = .multisampling2X
        return view
    }
    func updateUIView(_ view: SCNView, context: Context) {
        context.coordinator.avatar.position = SCNVector3(x, 0.85, z)
        context.coordinator.avatar.eulerAngles.y = heading
    }
    static func dismantleUIView(_ view: SCNView, coordinator: Coordinator) {
        view.isPlaying = false; view.scene = nil
    }
}
