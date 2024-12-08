package dev.jyotiraditya.sdfplayer

import android.app.Application
import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.ktor.client.HttpClient
import io.ktor.client.request.get
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

private val Context.dataStore by preferencesDataStore(name = "settings")
private val IP_KEY = stringPreferencesKey("ip_address")

data class UiState(
    val savedIp: String = "",
    val editingIp: String = "",
    val isEditing: Boolean = false,
    val errorMessage: String = "",
    val volume: Int = 15,
    val selectedTrack: Int = 1,
    val repeat: Boolean = false
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val client = HttpClient()
    private val _uiState = MutableStateFlow(UiState())
    val uiState: StateFlow<UiState> = _uiState.asStateFlow()

    init {
        viewModelScope.launch {
            getApplication<Application>().dataStore.data.collect { preferences ->
                preferences[IP_KEY]?.let { savedIp ->
                    _uiState.update { it.copy(savedIp = savedIp) }
                }
            }
        }
    }

    fun startEditing() {
        _uiState.update {
            it.copy(
                isEditing = true,
                editingIp = it.savedIp
            )
        }
    }

    fun updateEditingIp(ip: String) {
        _uiState.update { it.copy(editingIp = ip) }
    }

    fun saveIp() {
        viewModelScope.launch {
            getApplication<Application>().dataStore.edit { preferences ->
                preferences[IP_KEY] = _uiState.value.editingIp
            }
            _uiState.update {
                it.copy(
                    savedIp = it.editingIp,
                    isEditing = false,
                    errorMessage = ""
                )
            }
        }
    }

    fun selectTrack(track: Int) {
        _uiState.update { it.copy(selectedTrack = track) }
    }

    fun setRepeat(repeat: Boolean) {
        _uiState.update { it.copy(repeat = repeat) }
    }

    fun playSelected() {
        controlPlayer("play?track=${_uiState.value.selectedTrack}&repeat=${if (_uiState.value.repeat) 1 else 0}")
    }

    fun stop() {
        controlPlayer("stop")
    }

    fun updateVolume(volume: Int) {
        _uiState.update { it.copy(volume = volume) }
    }

    fun setVolume() {
        controlPlayer("volume?level=${_uiState.value.volume}")
    }

    private fun controlPlayer(action: String) {
        viewModelScope.launch {
            try {
                val response = client.get("http://${_uiState.value.savedIp}/$action")
                if (response.status.value in 200..299) {
                    _uiState.update { it.copy(errorMessage = "") }
                } else {
                    _uiState.update { it.copy(errorMessage = "Error: ${response.status.value}") }
                }
            } catch (e: Exception) {
                _uiState.update { it.copy(errorMessage = "Error: ${e.localizedMessage}") }
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        client.close()
    }
}