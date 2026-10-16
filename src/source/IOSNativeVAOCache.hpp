#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// VAO 0 remains the renderer's canonical mutable vertex state. Each cached VAO
// captures an immutable copy for a draw, and draw() always restores VAO 0.
// No GL queries, client-pointer arrays, or assumed buffer-name generations.
template<std::size_t Sets = 64, std::size_t Ways = 4>
class IOSNativeVAOCache {
public:
    struct Attribute {
        std::uint32_t buffer = 0, stride = 0, type = 0, components = 0;
        std::uintptr_t offset = 0;
        bool normalized = false;
        bool operator==(const Attribute &b) const {
            return buffer == b.buffer && stride == b.stride && type == b.type &&
                components == b.components && offset == b.offset && normalized == b.normalized;
        }
    };
    struct Key {
        std::uint32_t mask = 0, indexBuffer = 0;
        std::array<Attribute, 16> attributes{};
        bool operator==(const Key &b) const {
            if (mask != b.mask || indexBuffer != b.indexBuffer) return false;
            for (unsigned i = 0; i < 16; ++i)
                if ((mask & (1u << i)) && !(attributes[i] == b.attributes[i])) return false;
            return true;
        }
        bool references(std::uint32_t name) const {
            if (indexBuffer == name) return true;
            for (unsigned i = 0; i < 16; ++i)
                if ((mask & (1u << i)) && attributes[i].buffer == name) return true;
            return false;
        }
    };
    struct Counters { std::uint64_t hits = 0, misses = 0, evictions = 0, invalidations = 0, bypasses = 0, admissions = 0; } counters;

    template<class GL>
    void draw(GL &gl, const Key &key, std::uint32_t canonicalArrayBuffer,
              std::uint32_t mode, int count, std::uint32_t type, const void *indices) {
        if (count <= 0 || !usable(key) || canonicalArrayBuffer == ~0u) {
            gl.glDrawElements(mode, count, type, indices);
            return;
        }
        const auto bucket = hash(key) % Sets;
        auto &set = entries_[bucket];
        Entry *selected = nullptr;
        for (auto &entry : set)
            if (entry.name && entry.key == key) { selected = &entry; break; }
        if (selected) {
            ++counters.hits;
            gl.glBindVertexArray(selected->name);
        } else {
            ++counters.misses;
            // One-off animated/dynamic offsets must never allocate GL objects.
            // Admit only a repeated unknown capture, bounded to one ghost/set.
            if (!ghostValid_[bucket] || !(ghosts_[bucket] == key)) {
                ghosts_[bucket] = key;
                ghostValid_[bucket] = true;
                ++counters.bypasses;
                gl.glDrawElements(mode, count, type, indices);
                return;
            }
            ghostValid_[bucket] = false;
            selected = &set[0];
            for (auto &entry : set) {
                if (!entry.name) { selected = &entry; break; }
                if (entry.used < selected->used) selected = &entry;
            }
            if (selected->name) {
                gl.glDeleteVertexArrays(1, &selected->name);
                ++counters.evictions;
            }
            selected->name = 0;
            gl.glGenVertexArrays(1, &selected->name);
            if (!selected->name) { gl.glDrawElements(mode, count, type, indices); return; }
            ++counters.admissions;
            selected->key = key;
            gl.glBindVertexArray(selected->name);
            gl.glBindBuffer(0x8893u, key.indexBuffer); // ELEMENT_ARRAY_BUFFER: VAO-local.
            for (unsigned i = 0; i < 16; ++i) {
                if (!(key.mask & (1u << i))) continue;
                const auto &a = key.attributes[i];
                gl.glBindBuffer(0x8892u, a.buffer);
                gl.glVertexAttribPointer(i, a.components, a.type, a.normalized,
                    a.stride, reinterpret_cast<const void *>(a.offset));
                gl.glEnableVertexAttribArray(i);
            }
            // ARRAY_BUFFER is global, unlike the element binding.
            gl.glBindBuffer(0x8892u, canonicalArrayBuffer);
        }
        selected->used = ++clock_;
        gl.glDrawElements(mode, count, type, indices);
        gl.glBindVertexArray(0);
    }

    template<class GL> void invalidateBuffer(GL &gl, std::uint32_t buffer) {
        if (!buffer) return;
        for (std::size_t i = 0; i < Sets; ++i)
            if (ghostValid_[i] && ghosts_[i].references(buffer)) ghostValid_[i] = false;
        for (auto &set : entries_) for (auto &entry : set)
            if (entry.name && entry.key.references(buffer)) {
                gl.glDeleteVertexArrays(1, &entry.name);
                entry.name = 0;
                ++counters.invalidations;
            }
    }
    template<class GL> void clear(GL &gl) {
        ghostValid_.fill(false);
        for (auto &set : entries_) for (auto &entry : set)
            if (entry.name) { gl.glDeleteVertexArrays(1, &entry.name); entry.name = 0; }
    }
private:
    static_assert(Sets > 0 && Ways > 0, "The bounded VAO cache must have capacity");
    struct Entry { Key key{}; std::uint32_t name = 0; std::uint64_t used = 0; };
    std::array<std::array<Entry, Ways>, Sets> entries_{};
    std::array<Key, Sets> ghosts_{};
    std::array<bool, Sets> ghostValid_{};
    std::uint64_t clock_ = 0;
    static bool usable(const Key &key) {
        if (!key.indexBuffer || (key.mask & ~0xffffu)) return false;
        for (unsigned i = 0; i < 16; ++i)
            if ((key.mask & (1u << i)) && !key.attributes[i].buffer) return false;
        return true;
    }
    static std::uint64_t hash(const Key &key) {
        std::uint64_t h = 1469598103934665603ull;
        auto add = [&h](std::uint64_t value) { h = (h ^ value) * 1099511628211ull; };
        add(key.mask); add(key.indexBuffer);
        for (unsigned i = 0; i < 16; ++i) if (key.mask & (1u << i)) {
            const auto &a = key.attributes[i];
            add(a.buffer); add(a.stride); add(a.type); add(a.components);
            add(a.offset); add(a.normalized);
        }
        // Aligned offsets and names must influence high and low bucket bits.
        h ^= h >> 33; h *= 0xff51afd7ed558ccdull;
        h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull;
        h ^= h >> 33;
        return h;
    }
};
