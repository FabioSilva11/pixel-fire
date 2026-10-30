plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.fabiosilva.ecosdeeldra"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.fabiosilva.ecosdeeldra"
        minSdk = 24
        targetSdk = 36
        versionCode = 13
        versionName = "0.10.0"

        ndk {
            // Celulares atuais (arm64). Compilar apenas arm64 economiza RAM e metade do tempo nativo.
            abiFilters += listOf("arm64-v8a")
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
        compose = true
    }

}

kotlin {
    jvmToolchain(17)
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2025.09.00")
    implementation(composeBom)
    implementation("androidx.activity:activity-compose:1.13.0")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.runtime:runtime")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("androidx.activity:activity-ktx:1.13.0")
    // O lint de release exige Fragment >= 1.3.0 para registerForActivityResult; a dependência transitiva trazia 1.1.0.
    implementation("androidx.fragment:fragment:1.8.6")
    testImplementation("junit:junit:4.13.2")
    // Implementação real de org.json para testes JVM (o android.jar só traz stubs que lançam "not mocked").
    testImplementation("org.json:json:20250517")
}
