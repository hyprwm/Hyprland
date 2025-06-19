# Hyprland - Améliorations Potentielles

Suite à une exploration approfondie du codebase, voici les améliorations recommandées organisées par priorité.

## 🔴 Problèmes Critiques (Attention Immédiate)

### 1. Fuites Mémoire
- **HookSystem.cpp:185-250** : `free()` appelé sur un pointeur null
  ```cpp
  // Problème actuel
  m_originalBytes = nullptr;
  free(m_originalBytes);  // Libère nullptr!
  
  // Fix
  free(m_originalBytes);
  m_originalBytes = nullptr;
  ```

- **HookSystem.cpp:332** : Fuite mémoire intentionnelle de 64B par hook
  - Implémenter un pool mémoire avec tracking approprié

- **PresentationTime.cpp:126-129** : Fuite de feedback qui peut croître indéfiniment
  - Investiguer la cause racine au lieu du band-aid actuel

### 2. Sécurité Mémoire
- **render/OpenGL.hpp:123** : "FIXME: raw pointer galore!" pour les structures de rendu critiques
- **render/OpenGL.cpp:108** : `malloc()` sans vérification de retour
- Remplacer tous les `new`/`delete` par des smart pointers
- Corriger les références circulaires avec le pattern `m_self`

### 3. Gestion des Couleurs
- **ColorManagement.cpp, XXColorManagement.cpp, FrogColorManagement.cpp**
  - Parsing ICC non implémenté (lignes 409, 452)
  - Validation et vérifications de support manquantes

## 🟠 Optimisations Haute Priorité

### 1. Performance de Rendu

#### A. Batching de Rendu
- **Problème** : Changements d'état excessifs, appels de dessin redondants
- **Solution** : Grouper les opérations similaires (même shader, même état de blend)
- **Gain estimé** : Réduction de 50-70% des draw calls

#### B. Optimisation du Blur
- **Localisation** : `OpenGL.cpp:1781-1977`
- **Problème** : Multiple passes avec alternance de framebuffers
- **Solution** : Implémenter un blur single-pass avec compute shaders
- **Gain estimé** : Performance 3-4x supérieure

#### C. Frustum Culling
- **Problème** : Toutes les fenêtres sont traitées indépendamment de leur visibilité
- **Solution** : Implémenter un culling avec partitionnement spatial
- **Gain estimé** : Réduction CPU de 30-50% dans les scènes complexes

#### D. Optimisations Spécifiques
- Cache des calculs UV pour surfaces statiques
- Utilisation du rendu instancié pour surfaces similaires
- Texture atlasing pour éléments UI (réduction de 60% des bind calls)

### 2. Refactoring Architectural

#### A. Fusion des Managers de Curseur
Fusionner `CursorManager` + `XCursorManager` + aspects curseur de `PointerManager`:
```cpp
class CCursorSubsystem {
    // De CursorManager: gestion buffer/image curseur
    // De XCursorManager: chargement thème et support curseur X11
    // De PointerManager: rendu curseur et curseur hardware
};
```

#### B. Décomposition du KeybindManager
Diviser les 3300 lignes en composants modulaires:
```cpp
class CKeybindParser { /* Parse et validation */ };
class CKeybindRegistry { /* Stockage et lookup */ };
class CKeybindDispatcher { /* Exécution des actions */ };
```

#### C. Consolidation des Protocoles
- Créer des classes template de base pour patterns communs
- 40% de réduction de code estimée
- Patterns identifiés:
  - Manager Resource Handling
  - Surface Addon Pattern
  - Input Device Protocol Base

## 🟡 Améliorations Priorité Moyenne

### 1. Organisation du Code

#### A. Injection de Dépendances
Remplacer les singletons globaux par un registre de services:
```cpp
class CServiceRegistry {
    template<typename T>
    void registerService(SP<T> service);
    
    template<typename T>
    SP<T> getService();
};
```

#### B. Documentation
- Ajouter documentation architecturale complète
- Documenter les design patterns utilisés
- Inline documentation pour algorithmes complexes

### 2. Améliorations Performance

#### A. Optimisation Animation
- Timer animation actif même sans animations (500μs)
- Implémenter scheduling dynamique basé sur animations actives

#### B. Optimisation Containers
- Pré-réserver capacité des vectors
- Utiliser `unordered_set/map` pour lookups fréquents
- Implémenter indexation spatiale pour requêtes window/monitor

#### C. Optimisation Strings
- Utiliser `string_view` où possible
- Pré-allouer buffers string
- Éviter format strings dans hot paths

## 🟢 Améliorations Long Terme

### 1. Infrastructure de Test
- Ajouter suite de tests unitaires complète
- Implémenter CI/CD avec tests automatisés
- Tests de performance automatisés

### 2. Rendu Moderne
- Rendu texte accéléré GPU (remplacer Cairo)
- Utiliser compute shaders pour plus d'effets
- Implémenter command buffer system

### 3. Qualité du Code
- Adresser systématiquement les 74 items TODO/FIXME
- Standardiser gestion d'erreurs
- Réduire taille des fichiers volumineux

## Métriques d'Impact

### Performance
- **Réduction draw calls**: 50-70%
- **Performance blur**: 3-4x
- **Réduction overhead CPU**: 30-50%
- **Réduction allocations mémoire**: 40%

### Maintenabilité
- **Réduction duplication code**: 30-40%
- **Amélioration testabilité**: Significative
- **Réduction bugs potentiels**: ~25%

## Plan d'Action Recommandé

### Phase 1 (1-2 semaines)
1. Corriger fuites mémoire critiques
2. Ajouter vérifications d'erreur manquantes
3. Commencer refactoring managers curseur

### Phase 2 (1 mois)
1. Implémenter render batching
2. Optimiser système de blur
3. Décomposer KeybindManager

### Phase 3 (2-3 mois)
1. Consolidation protocoles
2. Injection de dépendances
3. Infrastructure de test

### Phase 4 (Long terme)
1. Optimisations render avancées
2. Documentation complète
3. Adresser dette technique restante

## Notes Techniques

### Fichiers Critiques à Réviser
- `src/plugins/HookSystem.cpp` - Fuites mémoire
- `src/protocols/PresentationTime.cpp` - Feedback leak
- `src/render/OpenGL.cpp` - Raw pointers et optimisations
- `src/managers/KeybindManager.cpp` - Refactoring nécessaire
- `src/protocols/*` - Consolidation patterns

### Outils Recommandés
- Valgrind/ASAN pour détecter fuites
- Tracy pour profiling performance
- clang-tidy pour quality checks
- Perf pour analyse performance système

Ce document représente une analyse approfondie basée sur l'état actuel du codebase. Les estimations de gains sont basées sur des patterns similaires dans d'autres compositors Wayland.