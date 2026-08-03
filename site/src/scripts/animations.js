// Lenis smooth scroll + GSAP ScrollTrigger - Client-side only
if (typeof window !== 'undefined') {
  Promise.all([
    import('lenis'),
    import('gsap'),
    import('gsap/ScrollTrigger')
  ]).then(([{ default: Lenis }, { gsap }, { ScrollTrigger }]) => {
    const lenis = new Lenis({
      duration: 1.2,
      easing: (t) => Math.min(1, 1.001 - Math.pow(2, -10 * t)),
      smooth: true,
      smoothTouch: false,
    });

    function raf(time) {
      lenis.raf(time);
      requestAnimationFrame(raf);
    }
    requestAnimationFrame(raf);

    gsap.registerPlugin(ScrollTrigger);

    gsap.utils.toArray('[data-animate="fade-up"]').forEach((el) => {
      gsap.from(el, {
        opacity: 0,
        y: 40,
        duration: 0.8,
        ease: 'power2.out',
        scrollTrigger: {
          trigger: el,
          start: 'top 85%',
          toggleActions: 'play none none reverse',
        },
        delay: parseFloat(el.dataset.delay || '0') / 1000,
      });
    });

    gsap.to('.hero-bg', {
      yPercent: 30,
      ease: 'none',
      scrollTrigger: {
        trigger: 'section:first-of-type',
        start: 'top top',
        end: 'bottom top',
        scrub: true,
      },
    });
  });
}