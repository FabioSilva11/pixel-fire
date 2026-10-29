plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.fabiosilva.ecosdeeldra"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.fabiosilva.ecosdeeldra"
        minSdk = 24
        targetSdk = 36
        versionCode = 12
        versionName = "0.9.0"

        ndk {
            // Celulares atuais (arm64) e antigos (armv7). Ao rodar pelo Android Studio,
            // só a ABI do aparelho conectado é compilada.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-fno-rtti")
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        buildConfig = true
    }

}

kotlin {
    jvmToolchain(17)
}

dependencies {
    implementation("androidx.activity:activity-ktx:1.13.0")
    // O lint de release exige Fragment >= 1.3.0 para registerForActivityResult; a dependência transitiva trazia 1.1.0.
    implementation("androidx.fragment:fragment:1.8.6")
    testImplementation("junit:junit:4.13.2")
    // Implementação real de org.json para testes JVM (o android.jar só traz stubs que lançam "not mocked").
    testImplementation("org.json:json:20250517")
}
