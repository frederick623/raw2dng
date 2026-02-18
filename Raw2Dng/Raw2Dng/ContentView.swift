//
//  ContentView.swift
//  Raw2DNG
//
//  Main interface for RAW to DNG conversion
//

import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @StateObject private var converter = Raw2DNGConverter()
    @State private var showingFolderPicker = false
    @State private var selectedFolderURL: URL?
    @State private var showingAlert = false
    @State private var alertMessage = ""
    
    var body: some View {
        NavigationView {
            VStack(spacing: 20) {
                // Header
                Text("RAW to DNG Converter")
                    .font(.largeTitle)
                    .fontWeight(.bold)
                    .padding(.top, 40)
                
                Spacer()
                
                // Selected folder display
                if let folderURL = selectedFolderURL {
                    VStack(spacing: 10) {
                        Text("Selected Folder:")
                            .font(.headline)
                        Text(folderURL.lastPathComponent)
                            .font(.body)
                            .foregroundColor(.secondary)
                            .lineLimit(2)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal)
                    }
                    .padding()
                    .background(Color.gray.opacity(0.1))
                    .cornerRadius(10)
                }
                
                // Select Folder Button
                Button(action: {
                    showingFolderPicker = true
                }) {
                    HStack {
                        Image(systemName: "folder.badge.plus")
                            .font(.title2)
                        Text("Select Folder")
                            .font(.headline)
                    }
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(10)
                }
                .padding(.horizontal, 40)
                
                // Convert Button
                Button(action: {
                    convertFiles()
                }) {
                    HStack {
                        Image(systemName: "arrow.triangle.2.circlepath")
                            .font(.title2)
                        Text("Convert Files")
                            .font(.headline)
                    }
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(selectedFolderURL != nil ? Color.green : Color.gray)
                    .foregroundColor(.white)
                    .cornerRadius(10)
                }
                .padding(.horizontal, 40)
                .disabled(selectedFolderURL == nil || converter.isConverting)
                
                // Progress view
                if converter.isConverting {
                    VStack(spacing: 10) {
                        ProgressView()
                            .progressViewStyle(CircularProgressViewStyle())
                        Text("Converting: \(converter.currentFile)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Text("\(converter.convertedCount) of \(converter.totalCount) files")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    .padding()
                }
                
                // Status message
                if !converter.statusMessage.isEmpty {
                    Text(converter.statusMessage)
                        .font(.body)
                        .foregroundColor(converter.hasError ? .red : .green)
                        .multilineTextAlignment(.center)
                        .padding()
                }
                
                Spacer()
                
                // Info text
                Text("Supported formats: CR2, NEF, ARW, DNG, and other RAW formats")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .padding()
            }
            .navigationBarHidden(true)
            .sheet(isPresented: $showingFolderPicker) {
                DocumentPicker(selectedURL: $selectedFolderURL)
            }
            .alert(isPresented: $showingAlert) {
                Alert(
                    title: Text("Conversion Complete"),
                    message: Text(alertMessage),
                    dismissButton: .default(Text("OK"))
                )
            }
        }
    }
    
    private func convertFiles() {
        guard let folderURL = selectedFolderURL else { return }
        
        converter.convertFiles(in: folderURL) { success, message in
            DispatchQueue.main.async {
                alertMessage = message
                showingAlert = true
            }
        }
    }
}

// Document Picker for selecting folders
struct DocumentPicker: UIViewControllerRepresentable {
    @Binding var selectedURL: URL?
    @Environment(\.presentationMode) var presentationMode
    
    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.folder], asCopy: false)
        picker.delegate = context.coordinator
        picker.allowsMultipleSelection = false
        picker.shouldShowFileExtensions = true
        return picker
    }
    
    func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}
    
    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }
    
    class Coordinator: NSObject, UIDocumentPickerDelegate {
        let parent: DocumentPicker
        
        init(_ parent: DocumentPicker) {
            self.parent = parent
        }
        
        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            guard let url = urls.first else { return }
            parent.selectedURL = url
            parent.presentationMode.wrappedValue.dismiss()
        }
        
        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            parent.presentationMode.wrappedValue.dismiss()
        }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
