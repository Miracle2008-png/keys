// Real Kotlin: coroutines, data classes, sealed interfaces, extensions.
package com.example.billing

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

sealed interface LoadState<out T> {
    data object Loading : LoadState<Nothing>
    data class Loaded<T>(val value: T) : LoadState<T>
    data class Failed(val cause: Throwable) : LoadState<Nothing>
}

data class Invoice(
    val id: Int,
    val customer: String,
    val total: Double = 0.0,
)

class InvoiceRepository(private val api: Api) {
    private val cache = mutableMapOf<Int, Invoice>()

    suspend fun find(id: Int): Invoice? = withContext(Dispatchers.IO) {
        cache[id] ?: runCatching { api.fetch(id) }
            .onSuccess { cache[id] = it }
            .getOrNull()
    }

    fun List<Invoice>.overdue(threshold: Double): List<Invoice> =
        filter { it.total > threshold }.sortedByDescending { it.total }

    companion object {
        const val DEFAULT_TIMEOUT = 30_000L
    }
}
