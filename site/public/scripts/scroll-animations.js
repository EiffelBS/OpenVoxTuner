// Advanced scroll animations with GSAP, Lenis, and ScrollTrigger
// Dramatic, immersive experience

(function() {
  'use strict';

  function init() {
    if (typeof window === 'undefined') return;
    if (window.__scrollAnimationsInitialized) return;
    window.__scrollAnimationsInitialized = true;

    Promise.all([
      import('lenis'),
      import('gsap'),
      import('gsap/ScrollTrigger')
    ]).then(([{ default: Lenis }, { gsap }, { ScrollTrigger }]) => {

      gsap.registerPlugin(ScrollTrigger);

      // ─── 1. LENIS SMOOTH SCROLL ───
      const lenis = new Lenis({
        duration: 1.4,
        easing: (t) => Math.min(1, 1.001 - Math.pow(2, -10 * t)),
        smooth: true,
        smoothTouch: false,
      });

      gsap.ticker.add((time) => {
        lenis.raf(time * 1000);
      });
      gsap.ticker.lagSmoothing(0);
      lenis.on('scroll', ScrollTrigger.update);

      // ─── 2. CURSOR GLOW ───
      const glow = document.createElement('div');
      glow.style.cssText = `
        position: fixed;
        width: 500px;
        height: 500px;
        border-radius: 50%;
        background: radial-gradient(circle, rgba(59,130,246,0.08) 0%, transparent 70%);
        pointer-events: none;
        z-index: 9998;
        transform: translate(-50%, -50%);
        transition: opacity 0.3s;
        opacity: 0;
      `;
      document.body.appendChild(glow);

      let glowVisible = false;
      document.addEventListener('mousemove', (e) => {
        if (!glowVisible) { glow.style.opacity = '1'; glowVisible = true; }
        gsap.to(glow, { x: e.clientX, y: e.clientY, duration: 0.6, ease: 'power2.out' });
      });
      document.addEventListener('mouseleave', () => {
        glow.style.opacity = '0';
        glowVisible = false;
      });

      // ─── 3. HERO ENTRANCE + SCROLL PARALLAX ───
      const hero = document.querySelector('.hero');
      if (hero) {
        const heroTitle = hero.querySelector('.hero-title');
        const heroSubtitle = hero.querySelector('.hero-subtitle');

        const tl = gsap.timeline({ defaults: { ease: 'power3.out' } });

        // Orbs pulse in
        tl.from('.hero-orb-1', { scale: 0, opacity: 0, duration: 1.5, ease: 'elastic.out(1, 0.5)' }, 0);
        tl.from('.hero-orb-2', { scale: 0, opacity: 0, duration: 1.5, ease: 'elastic.out(1, 0.5)' }, 0.2);
        tl.from('.hero-orb-3', { scale: 0, opacity: 0, duration: 1.5, ease: 'elastic.out(1, 0.5)' }, 0.4);

        // Badge drops in
        tl.from('.hero-badge', { y: -40, opacity: 0, duration: 0.8 }, 0.3);

        // Title scale + fade
        if (heroTitle) {
          tl.from(heroTitle, { y: 80, opacity: 0, scale: 0.85, duration: 1.2 }, 0.4);
        }

        // Subtitle word-by-word
        let originalSubtitleText = '';
        if (heroSubtitle) {
          originalSubtitleText = heroSubtitle.textContent.trim();
          const words = originalSubtitleText.split(/\s+/);
          heroSubtitle.innerHTML = words.map(w => `<span class="word" style="display:inline-block; opacity:0; transform:translateY(20px)">${w}</span>`).join(' ');
          tl.to('.hero-subtitle .word', {
            opacity: 1, y: 0, duration: 0.5, stagger: 0.04, ease: 'power2.out'
          }, 0.7);
        }

        // CTAs
        tl.from('.hero-ctas', { y: 40, opacity: 0, duration: 0.8 }, 1.0);

        // Tech badges stagger
        tl.from('.hero-badges span', { y: 20, opacity: 0, scale: 0.8, duration: 0.5, stagger: 0.08 }, 1.1);

        // Scroll indicator
        tl.from('.hero-scroll', { opacity: 0, y: -20, duration: 0.6 }, 1.3);

        // ─── Hero SCROLL PARALLAX — created AFTER entrance finishes ───
        tl.eventCallback('onComplete', () => {
          // Clear any leftover inline transforms from entrance
           gsap.set([heroTitle, '.hero-badge', '.hero-ctas', '.hero-badges span', '.hero-scroll'], { clearProps: 'all' });
           // Restore original subtitle text (remove word-by-word spans)
           if (heroSubtitle && originalSubtitleText) {
             heroSubtitle.textContent = originalSubtitleText;
           }

          // Orbs parallax (only transform, no opacity conflict)
          gsap.to('.hero-orb-1', {
            y: -200, x: -100,
            scrollTrigger: { trigger: hero, start: 'top top', end: 'bottom top', scrub: 1 }
          });
          gsap.to('.hero-orb-2', {
            y: -150, x: 80,
            scrollTrigger: { trigger: hero, start: 'top top', end: 'bottom top', scrub: 1 }
          });
          gsap.to('.hero-orb-3', {
            y: -300,
            scrollTrigger: { trigger: hero, start: 'top top', end: 'bottom top', scrub: 1 }
          });

          // Title parallax — moves up and fades
          if (heroTitle) {
            gsap.to(heroTitle, {
              yPercent: -30, opacity: 0,
              scrollTrigger: { trigger: hero, start: 'center center', end: 'bottom top', scrub: 1 }
            });
          }

          // Whole hero fades out as you scroll past
          gsap.to(hero, {
            opacity: 0,
            scrollTrigger: { trigger: hero, start: '60% center', end: 'bottom top', scrub: 1 }
          });
        });
      }

      // ─── 4. STATS COUNTERS ───
      document.querySelectorAll('.stat-number[data-count]').forEach((el) => {
        const target = parseInt(el.dataset.count, 10);
        const obj = { val: 0 };
        gsap.to(obj, {
          val: target,
          duration: 2,
          ease: 'power2.out',
          scrollTrigger: {
            trigger: el,
            start: 'top 85%',
            toggleActions: 'play none none reverse',
          },
          onUpdate: () => {
            el.textContent = Math.round(obj.val);
          }
        });
      });

      // Stat items reveal
      gsap.from('.stat-item', {
        y: 60, opacity: 0, scale: 0.9, duration: 0.8, stagger: 0.15, ease: 'power3.out',
        scrollTrigger: { trigger: '.stats', start: 'top 80%', toggleActions: 'play none none reverse' }
      });

      // ─── 5. FEATURES SECTION ───
      const featuresSection = document.querySelector('.features');
      if (featuresSection) {
        // Title dramatic reveal
        const featTitle = featuresSection.querySelector('.features-title');
        if (featTitle) {
          gsap.from(featTitle, {
            y: 80, opacity: 0, scale: 0.9, duration: 1, ease: 'power3.out',
            scrollTrigger: { trigger: featTitle, start: 'top 85%', toggleActions: 'play none none reverse' }
          });
        }

        // Cards stagger with scale + rotation
        const cards = featuresSection.querySelectorAll('.feature-card');
        gsap.from(cards, {
          y: 100, opacity: 0, scale: 0.85, rotateX: 15, duration: 0.8,
          stagger: { amount: 0.6, from: 'start' },
          ease: 'power3.out',
          scrollTrigger: { trigger: featuresSection.querySelector('.grid'), start: 'top 85%', toggleActions: 'play none none reverse' }
        });

        // Icon pop-in
        const icons = featuresSection.querySelectorAll('.feature-icon');
        gsap.from(icons, {
          scale: 0, rotation: -180, duration: 0.6, stagger: 0.1, ease: 'back.out(2)',
          scrollTrigger: { trigger: featuresSection.querySelector('.grid'), start: 'top 80%', toggleActions: 'play none none reverse' }
        });
      }

      // ─── 6. 3D TILT ON FEATURE CARDS ───
      document.querySelectorAll('[data-tilt]').forEach((card) => {
        card.style.transformStyle = 'preserve-3d';
        card.style.perspective = '1000px';

        card.addEventListener('mousemove', (e) => {
          const rect = card.getBoundingClientRect();
          const x = (e.clientX - rect.left) / rect.width - 0.5;
          const y = (e.clientY - rect.top) / rect.height - 0.5;
          gsap.to(card, {
            rotateY: x * 12,
            rotateX: -y * 12,
            duration: 0.4,
            ease: 'power2.out',
          });
        });

        card.addEventListener('mouseleave', () => {
          gsap.to(card, { rotateX: 0, rotateY: 0, duration: 0.6, ease: 'elastic.out(1, 0.5)' });
        });
      });

      // ─── 7. SHOWCASE SCROLL ───
      const showcasePinned = document.querySelector('.showcase-pinned');
      if (showcasePinned) {
        const slides = document.querySelectorAll('.showcase-slide');
        const dots = document.querySelectorAll('.showcase-dot');
        const counter = document.querySelector('[data-counter-current]');
        const totalSlides = slides.length;

        // Hold and crossfade durations (in timeline units)
        const holdDuration = 2;
        const fadeDuration = 2;
        const entranceDuration = 1;

        // Total: entrance + (N holds) + (N-1 fades)
        const totalDuration = entranceDuration + totalSlides * holdDuration + (totalSlides - 1) * fadeDuration;

        const tl = gsap.timeline({
          scrollTrigger: {
            trigger: showcasePinned,
            start: 'top top',
            end: `+=${totalDuration * 100}vh`,
            pin: '.showcase-sticky',
            scrub: 1,
            anticipatePin: 1,
          }
        });

        // 1. Entrance: slide 0 fades in from below (no separate ScrollTrigger)
        tl.fromTo(slides[0],
          { opacity: 0, y: 60, scale: 0.95 },
          { opacity: 1, y: 0, scale: 1, duration: entranceDuration, ease: 'power3.out' },
          0
        );

        // Header fades out after entrance
        tl.to('.showcase-header', { opacity: 0, y: -30, duration: 0.5 }, entranceDuration + holdDuration * 0.75);

        // 2. Per-slide: hold, then crossfade to next
        for (let i = 0; i < totalSlides; i++) {
          const cursor = tl.duration();

          // Hold: slide i stays visible
          tl.to({}, { duration: holdDuration }, cursor);

          // Crossfade to next slide (sequential: fade-out then fade-in)
          if (i < totalSlides - 1) {
            const cursor2 = tl.duration();

            // Fade out current slide (first half of transition)
            tl.to(slides[i], {
              opacity: 0, y: -20,
              duration: fadeDuration * 0.5,
              ease: 'power2.in',
              onComplete: () => { slides[i].style.pointerEvents = 'none'; }
            }, cursor2);

            // Fade in next slide (second half — starts AFTER fade-out completes)
            tl.fromTo(slides[i + 1],
              { opacity: 0, y: 20, pointerEvents: 'none' },
              { opacity: 1, y: 0, pointerEvents: 'auto', duration: fadeDuration * 0.5, ease: 'power2.out' },
              cursor2 + fadeDuration * 0.5
            );

            // Update dots and counter after crossfade
            tl.call(() => {
              dots.forEach((dot, d) => {
                if (d === i + 1) {
                  dot.classList.add('bg-white', 'w-8');
                  dot.classList.remove('bg-gray-600', 'w-2');
                } else {
                  dot.classList.remove('bg-white', 'w-8');
                  dot.classList.add('bg-gray-600', 'w-2');
                }
              });
              if (counter) counter.textContent = i + 2;
            }, null, cursor2 + fadeDuration);
          }
        }
      }

      // ─── 8. DOWNLOAD SECTION ───
      const downloadSection = document.querySelector('.download');
      if (downloadSection) {
        const dlTitle = downloadSection.querySelector('.download-title');
        if (dlTitle) {
          gsap.from(dlTitle, {
            y: 60, opacity: 0, scale: 0.9, duration: 1, ease: 'power3.out',
            scrollTrigger: { trigger: dlTitle, start: 'top 85%', toggleActions: 'play none none reverse' }
          });
        }

        const dlCards = downloadSection.querySelectorAll('.download-card');
        gsap.from(dlCards, {
          y: 80, opacity: 0, scale: 0.9, duration: 0.8, stagger: 0.2, ease: 'power3.out',
          scrollTrigger: { trigger: dlCards[0], start: 'top 85%', toggleActions: 'play none none reverse' }
        });
      }

      // ─── 9. SUPPORT SECTION ───
      const supportSection = document.querySelector('.support');
      if (supportSection) {
        const supportEls = supportSection.querySelectorAll('.support-animate');
        if (supportEls.length) {
          gsap.to(supportEls, {
            y: 0, opacity: 1, scale: 1, duration: 0.8, ease: 'power3.out',
            scrollTrigger: { trigger: supportSection, start: 'top 75%', toggleActions: 'play none none none' }
          });
        }
      }

      // ─── 8. MAGNETIC BUTTONS ───
      document.querySelectorAll('[data-magnetic]').forEach((btn) => {
        btn.addEventListener('mousemove', (e) => {
          const rect = btn.getBoundingClientRect();
          const x = e.clientX - rect.left - rect.width / 2;
          const y = e.clientY - rect.top - rect.height / 2;
          gsap.to(btn, { x: x * 0.3, y: y * 0.3, duration: 0.3, ease: 'power2.out' });
        });
        btn.addEventListener('mouseleave', () => {
          gsap.to(btn, { x: 0, y: 0, duration: 0.5, ease: 'elastic.out(1, 0.5)' });
        });
      });

      // ─── 9. PROGRESS BAR ───
      const progressBar = document.createElement('div');
      progressBar.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 3px;
        background: linear-gradient(90deg, #3b82f6, #8b5cf6, #06b6d4);
        transform-origin: 0% 50%;
        transform: scaleX(0);
        z-index: 9999;
        pointer-events: none;
      `;
      document.body.appendChild(progressBar);

      ScrollTrigger.create({
        trigger: document.body,
        start: 'top top',
        end: 'bottom bottom',
        onUpdate: (self) => {
          gsap.set(progressBar, { scaleX: self.progress });
        },
      });

      // ─── 10. REFRESH ON RESIZE ───
      let resizeTimeout;
      window.addEventListener('resize', () => {
        clearTimeout(resizeTimeout);
        resizeTimeout = setTimeout(() => ScrollTrigger.refresh(), 250);
      });

      console.log('✅ Scroll animations initialized');
    }).catch((err) => {
      console.warn('Scroll animations failed to load:', err);
    });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
